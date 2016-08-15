/*
 * ---------------------------------- LICENSE ----------------------------------
 * This software is in the public domain.  Where that dedication is not
 * recognized, you are granted a perpetual, irrevocable license to copy,
 * distribute, and modify the source code as you see fit.
 *
 * The source code is provided "as is", without warranty of any kind, express
 * or implied. No attribution is required, but always appreciated.
 * =============================================================================
 *
 */

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#include <cuda.h>
#include <math_functions.h>
#include <vector_types.h>
#include <vector_functions.h>
#include <cuda_profiler_api.h>
#include <cuda_gl_interop.h>
#include <helper_math.h>

#include <stdint.h>
typedef int32_t i32;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t usize;
typedef int32_t b32;

#include <float.h>

typedef float f32;

#define F32_MAX FLT_MAX

#define HALF_PI32 1.570796327f

typedef struct {
    float3 pos;
    float3 dir;
} rtRay;

typedef struct {
    float3 pos;
    f32 r;
    float3 diffuse;
} rtSphere;

typedef struct {
    float3 pos;
    float3 emiss;
} rtLight;

#define TEX_PITCH 4
#define SAMPLE_COUNT 8
#define R_SAMPLE_COUNT 1.0f / (f32) SAMPLE_COUNT
#define BLOCK_COUNT 8

#define LIGHT_COUNT 1
static rtLight h_light[LIGHT_COUNT] = {
    { make_float3(0.0f, -15.0f, 0.0f), make_float3(1.0f) },
};

#define SPHERE_COUNT 6
static rtSphere h_spheres[SPHERE_COUNT] = {
    { make_float3(0.0f, 0.0f, 0.0f), 1.0f, make_float3(0.5f) },
    { make_float3(0.0f, 6.0f, 0.0f), 4.0f, make_float3(1.0f, 0.0f, 0.0f) },
    { make_float3(0.0f, -6.0f, 0.0f), 3.0f, make_float3(1.0f) },
    { make_float3(6.0f, 0.0f, -5.0f), 3.0f, make_float3(0.0f, 1.0f, 0.0f) },
    { make_float3(-6.0f, 0.0f, 0.0f), 3.0f, make_float3(0.0f, 0.0f, 1.0f) },
    { make_float3(0.0f, 10010.0f, 0.0f), 10000.0f, make_float3(0.75f) },
};

static u32 gl_tex;
static cudaGraphicsResource* d_resource = NULL;
__constant__ rtSphere d_spheres[SPHERE_COUNT];
__constant__ rtLight d_light[LIGHT_COUNT];

__device__ inline void ray_project(rtRay* ray, u32 px, u32 py, i32 width, i32 height, f32 fov) {
    f32 ar = __fdividef((f32) width, (f32) height);
    f32 a = __tanf(HALF_PI32 * fov / 180.0f);
    f32 xx = (2.0f * ((px + 0.5f) * __fdividef(1.0f, (f32) width)) - 1) * a * ar;
    f32 yy = (1.0f - 2.0f * ((py + 0.5f) * __fdividef(1.0f, (f32) height))) * a;
    ray->pos = make_float3(0.0f, 0.0f, 20.0f);
    ray->dir = normalize(make_float3(xx, yy, -1.0f));
}

__device__ inline b32 ray_sphere_intersect(rtRay* ray, rtSphere* s, f32* dist) {
    float3 light_dir = s->pos - ray->pos;
    f32 sz = fmaxf(0.0f, dot(light_dir, ray->dir));
    f32 dsq = dot(light_dir, light_dir) - __powf(sz, 2);
    f32 rsq = __powf(s->r, 2);
    f32 q = __fsqrt_rn(rsq - dsq);
    *dist = sz - q;
    return dsq < rsq;
}

__global__ void raytrace_kernel(cudaSurfaceObject_t surf, i32 width, i32 height, f32 fov) {
    const u32 x = blockIdx.x * blockDim.x + threadIdx.x;
    const u32 y = blockIdx.y * blockDim.y + threadIdx.y;

    __shared__ float3 pixels[BLOCK_COUNT][BLOCK_COUNT];
    pixels[threadIdx.x][threadIdx.y] = make_float3(0.0f);

    __shared__ rtRay rays[BLOCK_COUNT][BLOCK_COUNT];
    ray_project(&rays[threadIdx.x][threadIdx.y], x, y, width, height, fov);

    f32 tn = F32_MAX;
    rtSphere* s = NULL;
#pragma unroll SPHERE_COUNT
    for(i32 i = 0; i < SPHERE_COUNT; i++) {
        f32 dist = F32_MAX;
        if(ray_sphere_intersect(&rays[threadIdx.x][threadIdx.y], &d_spheres[i], &dist)) {
            if(dist < tn) {
                tn = dist;
                s = &d_spheres[i];
            }
        }
    }

    __syncthreads();
    if(s) {
        float3 phit = rays[threadIdx.x][threadIdx.y].pos + rays[threadIdx.x][threadIdx.y].dir * tn;
        float3 nhit = normalize(phit - s->pos);
#pragma unroll LIGHT_COUNT
        for(u32 i = 0; i < LIGHT_COUNT; i++) {
            float3 tr = make_float3(1.0f);
#pragma unroll SAMPLE_COUNT
            for(u32 sa = 0; sa < SAMPLE_COUNT; sa++) {
                float3 ld = normalize(d_light[i].pos + __fdividef(sa, (f32) SAMPLE_COUNT) - phit);
                rtRay light_ray = { phit + nhit * 0.01f, ld };
#pragma unroll SPHERE_COUNT
                for(u32 j = 0; j < SPHERE_COUNT; j++) {
                    f32 dist;
                    if(ray_sphere_intersect(&light_ray, &d_spheres[j], &dist)) {
                        tr = make_float3(0.1f);
                        break;
                    }
                }
                pixels[threadIdx.x][threadIdx.y] += (s->diffuse * tr * fmaxf(0.0f, dot(nhit, ld)) * d_light[i].emiss) * R_SAMPLE_COUNT;
            }
        }
        pixels[threadIdx.x][threadIdx.y] *= s->diffuse;
    }

    pixels[threadIdx.x][threadIdx.y] = clamp(pixels[threadIdx.x][threadIdx.y] * 255.0f, 0.0f, 255.0f);
    __syncthreads();
    surf2Dwrite(
            (0xFF000000 |
             (((u32) pixels[threadIdx.x][threadIdx.y].x) << 16) |
             (((u32) pixels[threadIdx.x][threadIdx.y].y) << 8) |
             (((u32) pixels[threadIdx.x][threadIdx.y].z) << 0)),
            surf, x * TEX_PITCH, y);
}

extern "C" void raytrace_init(i32 width, i32 height) {
    glEnable(GL_TEXTURE_2D);

    glGenTextures(1, &gl_tex);

    glBindTexture(GL_TEXTURE_2D, gl_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    cudaGraphicsGLRegisterImage(&d_resource, gl_tex, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard);

    cudaMemcpyToSymbol(d_spheres, h_spheres, SPHERE_COUNT * sizeof(rtSphere));
    cudaMemcpyToSymbol(d_light, h_light, LIGHT_COUNT * sizeof(rtLight));

    cudaProfilerStart();
}

extern "C" void raytrace_destroy() {
    cudaProfilerStop();
}

extern "C" void raytrace_resize(i32 width, i32 height) {
    glViewport(0, 0, width, height);
    cudaGraphicsUnregisterResource(d_resource);
    glBindTexture(GL_TEXTURE_2D, gl_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
    cudaGraphicsGLRegisterImage(&d_resource, gl_tex, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard);
}

extern "C" void raytrace_render(i32 width, i32 height, f32 t, f32 dt, f32 fov) {
    dim3 block = dim3(BLOCK_COUNT, BLOCK_COUNT, 1);
    dim3 grid = dim3(width / block.x, height / block.y, 1);

    cudaGraphicsMapResources(1, &d_resource);
    cudaArray_t array;
    cudaGraphicsSubResourceGetMappedArray(&array, d_resource, 0, 0);
    cudaResourceDesc desc;
    desc.resType = cudaResourceTypeArray;
    desc.res.array.array = array;
    cudaSurfaceObject_t surf;
    cudaCreateSurfaceObject(&surf, &desc);

    h_light[0].pos.x = sinf(t) * 20.0f;
    h_light[0].pos.z = cosf(t) * -20.0f;
    cudaMemcpyToSymbol(d_light, h_light, LIGHT_COUNT * sizeof(rtLight));

    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);

    cudaEventRecord(start);
    raytrace_kernel<<<grid, block>>>(surf, width, height, fov);
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);

    f32 ms = 0;
    cudaEventElapsedTime(&ms, start, stop);

    cudaDestroySurfaceObject(surf);
    cudaGraphicsUnmapResources(1, &d_resource);
    cudaDeviceSynchronize();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gl_tex);

    glBegin(GL_QUADS); {
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(1.0f, 1.0f);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(-1.0f, 1.0f);
    } glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}
