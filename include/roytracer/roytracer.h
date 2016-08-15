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

#ifndef RT_H
#define RT_H

#define RT_VERSION_MAJOR 0
#define RT_VERSION_MINOR 1
#define RT_VERSION_PATCH 0

#include <glad/glad.h>

#include <kj/kj.h>
#include <kj/kj_math.h>

#define GLSL(v, s) cast(char*, v #s)

#define RT_TIMESTEP (1.0f / 240.0f)

typedef enum {
    TEX_TYPE_TEXTURE_1D = GL_TEXTURE_1D,
    TEX_TYPE_TEXTURE_2D = GL_TEXTURE_2D,
    TEX_TYPE_TEXTURE_3D = GL_TEXTURE_3D,
    TEX_TYPE_CUBEMAP = GL_TEXTURE_CUBE_MAP,
} rtTexType;

typedef struct {
    u32 id;
    rtTexType target;
} rtTex;

typedef struct {
    i32 w;
    i32 h;
    f32 t;
    f32 dt;
    u64 framecounter;
} rtState;

typedef enum {
    RT_EVENT_UNKNOWN,
    RT_EVENT_WINDOW_RESIZE,
    RT_EVENT_KEYPRESS,
    RT_EVENT_MOUSE_MOVE,
    RT_EVENT_MOUSE_EVENT,
} rtEvent;

#define RT_INIT(name) void name(void)
typedef RT_INIT(rtInitFn);

#define RT_EVENT(name) void name(rtEvent e, f32 d1, f32 d2, f32 d3)
typedef RT_EVENT(rtEventFn);

#define RT_UPDATE(name) void name(f32 t, f32 dt)
typedef RT_UPDATE(rtUpdateFn);

#define RT_PRE_UPDATE(name) void name()
typedef RT_PRE_UPDATE(rtPreUpdateFn);

#define RT_POST_UPDATE(name) void name()
typedef RT_POST_UPDATE(rtPostUpdateFn);

#define RT_RENDER(name) void name(void)
typedef RT_RENDER(rtRenderFn);

#define RT_EXIT(name) void name(void)
typedef RT_EXIT(rtExitFn);

void rt_set_args(i32, char**);
void rt_set_init(rtInitFn);
void rt_set_event(rtEventFn);
void rt_set_update(rtUpdateFn);
void rt_set_render(rtRenderFn);
void rt_set_exit(rtExitFn);
i32 rt_run(i32, i32, f32);

typedef enum {
    RT_ACTION_RELEASE = 0,
    RT_ACTION_PRESS = 1,
    RT_ACTION_REPEAT = 2,
} rtAction;

typedef enum {
    RT_BUTTON_LEFT = 0,
    RT_BUTTON_RIGHT = 1,
    RT_BUTTON_MIDDLE = 2,
} rtBtn;

typedef enum {
    RT_KEY_W,
    RT_KEY_ESCAPE
} rtKey;

f32 rt_get_time();
rtAction rt_get_key(rtKey key);

void raytrace_init(i32 width, i32 height);
void raytrace_destroy();
void raytrace_resize(i32 width, i32 height);
void raytrace_render(i32 width, i32 height, f32 t, f32 dt, f32 fov);

#endif
