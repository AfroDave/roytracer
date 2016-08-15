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

#if defined(RT_OMP)
#include <omp.h>
#endif

#include <roytracer/roytracer.h>

typedef struct {
    kjVec3f pos;
    kjVec3f dir;
} rtRay;

typedef struct {
    kjVec3f pos;
    f32 r;
    kjVec3f diffuse;
} rtSphere;

typedef struct {
    kjVec3f pos;
    kjVec3f emiss;
} rtLight;

#define TEX_PITCH 4
#define SAMPLE_COUNT 1
#define R_SAMPLE_COUNT 1.0f / (f32) SAMPLE_COUNT
#define BLOCK_COUNT 8

#define LIGHT_COUNT 1
static rtLight lights[LIGHT_COUNT] = {
    { { 0.0f, -15.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
};

#define SPHERE_COUNT 6
static rtSphere spheres[SPHERE_COUNT] = {
    { { 0.0f, 0.0f, 0.0f }, 1.0f, { 0.5f, 0.5f, 0.5f } },
    { { 0.0f, 6.0f, 0.0f }, 4.0f, { 1.0f, 0.0f, 0.0f } },
    { { 0.0f, -6.0f, 0.0f }, 3.0f, { 1.0f, 1.0f, 1.0f } },
    { { 6.0f, 0.0f, -5.0f }, 3.0f, { 0.0f, 1.0f, 0.0f } },
    { { -6.0f, 0.0f, 0.0f }, 3.0f, { 0.0f, 0.0f, 1.0f } },
    { { 0.0f, 10010.0f, 0.0f }, 10000.0f, { 0.75f, 0.75f, 0.75f } },
};

static u32 gl_tex;
static u32* framebuffer;

KJ_INLINE void ray_project(rtRay* ray, u32 px, u32 py, i32 width, i32 height, f32 fov) {
    f32 ar = ((f32) width / (f32) height);
    f32 a = kj_tanf((KJ_PI * 0.5f) * fov / 180.0f);
    f32 xx = (2.0f * ((px + 0.5f) * (1.0f / (f32) width)) - 1) * a * ar;
    f32 yy = (1.0f - 2.0f * ((py + 0.5f) * (1.0f / (f32) height))) * a;
    ray->pos = kj_vec3f(0.0f, 0.0f, 20.0f);
    ray->dir = kj_vec3f_normalise(kj_vec3f(xx, yy, -1.0f));
}

KJ_INLINE b32 ray_sphere_intersect(rtRay* ray, rtSphere* s, f32* dist) {
    kjVec3f ld = kj_vec3f_sub(s->pos, ray->pos);
    f32 sz = kj_maxf(0.0f, kj_vec3f_dot(ld, ray->dir));
    f32 dsq = kj_vec3f_dot(ld, ld) - kj_powf(sz, 2);
    f32 rsq = kj_powf(s->r, 2);
    f32 q = kj_sqrtf(rsq - dsq);
    *dist = sz - q;
    return dsq < rsq;
}

static void raytrace(u32* output, i32 y, i32 width, i32 height, f32 fov) {
    i32 x = 0;
#if defined(RT_OMP)
#pragma omp parallel default(none) private(x) num_threads(2)
#pragma omp for
#endif
    for(x = 0; x < width; x++) {
        kjVec3f pixel = kj_vec3f(0.0f, 0.0f, 0.0f);

        rtRay ray;
        ray_project(&ray, x, y, width, height, fov);

        f32 tn = F32_MAX;
        rtSphere* s = NULL;
        for(i32 i = 0; i < SPHERE_COUNT; i++) {
            f32 dist = F32_MAX;
            if(ray_sphere_intersect(&ray, &spheres[i], &dist)) {
                if(dist < tn) {
                    tn = dist;
                    s = &spheres[i];
                }
            }
        }

        if(s) {
            kjVec3f phit = kj_vec3f_add(ray.pos, kj_vec3f_mulf(ray.dir, tn));
            kjVec3f nhit = kj_vec3f_normalise(kj_vec3f_sub(phit, s->pos));
            for(u32 i = 0; i < LIGHT_COUNT; i++) {
                kjVec3f tr = kj_vec3f(1.0f, 1.0f, 1.0f);
                for(u32 sa = 0; sa < SAMPLE_COUNT; sa++) {
                    kjVec3f ld = kj_vec3f_normalise(kj_vec3f_sub(kj_vec3f_add(lights[i].pos, kj_vec3f_all(sa / (f32) SAMPLE_COUNT)), phit));
                    rtRay light_ray;
                    light_ray.pos = kj_vec3f_add(phit, kj_vec3f_mulf(nhit, 0.01f));
                    light_ray.dir = ld;
                    for(u32 j = 0; j < SPHERE_COUNT; j++) {
                        f32 dist;
                        if(ray_sphere_intersect(&light_ray, &spheres[j], &dist)) {
                            tr = kj_vec3f(0.1f, 0.1f, 0.1f);
                            break;
                        }
                    }
                    pixel = kj_vec3f_add(
                            pixel,
                            kj_vec3f_mulf(
                                kj_vec3f_mul(
                                    kj_vec3f_mulf(
                                        kj_vec3f_mul(s->diffuse, tr),
                                        kj_maxf(0.0f, kj_vec3f_dot(nhit, ld))), lights[i].emiss), R_SAMPLE_COUNT));
                }
            }
            pixel = kj_vec3f_mul(pixel, s->diffuse);
        }

        pixel = kj_vec3f_clamp(kj_vec3f_mulf(pixel, 255.0f), kj_vec3f_zero(), kj_vec3f(255.0f, 255.0f, 255.0f));
        output[x + width * y] = (0xFF000000 | (((u32) pixel.x) << 16) | (((u32) pixel.y) << 8) | (((u32) pixel.z) << 0));
    }
}

void raytrace_init(i32 width, i32 height) {
    framebuffer = kj_zalloc(width * height * kj_isize_of(u32));
    glEnable(GL_TEXTURE_2D);

    glGenTextures(1, &gl_tex);

    glBindTexture(GL_TEXTURE_2D, gl_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void raytrace_destroy() {
    kj_free(framebuffer);
}

void raytrace_resize(i32 width, i32 height) {
    glViewport(0, 0, width, height);
    glBindTexture(GL_TEXTURE_2D, gl_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
    framebuffer = kj_realloc(framebuffer, width * height * kj_isize_of(u32));
}

void raytrace_render(i32 width, i32 height, f32 t, f32 dt, f32 fov) {
    kj_unused(dt);
    lights[0].pos.x = kj_sinf(t) * 20.0f;
    lights[0].pos.z = kj_cosf(t) * -20.0f;
    i32 y = 0;

    {
#if defined(RT_OMP)
#pragma omp parallel default(none) private(y) num_threads(2)
#pragma omp for
#endif
        for(y = 0; y < height; y++) {
            raytrace(framebuffer, y, width, height, fov);
        }
    }

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gl_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, framebuffer);

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
