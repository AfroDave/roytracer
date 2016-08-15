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

#define RT_WINDOW_WIDTH 1024
#define RT_WINDOW_HEIGHT 1024
#include <roytracer/roytracer.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

typedef struct {
    i32 w;
    i32 h;

    f32 timestep;
    f32 time;
    f32 curr_time;
    f32 acc_time;

    rtInitFn* init;
    rtEventFn* event;
    rtUpdateFn* update;
    rtRenderFn* render;
    rtExitFn* exit;

    GLFWwindow* window;
} rtPlatform;

static rtPlatform platform = {0};

static void glfw_err_callback(i32 err, const char* desc) {
    kj_unused(err);
    kj_printf("%s\n", desc);
}

static void glfw_key_callback(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods) {
    kj_unused(window);
    kj_unused(scancode);
    kj_unused(mods);
    if((key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE) && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, KJ_TRUE);
    }
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        platform.event(RT_EVENT_KEYPRESS, kj_cast(f32, key), 0.0f, 0.0f);
    }
}

static void glfw_window_size_callback(GLFWwindow* window, i32 width, i32 height) {
    kj_unused(window);
    platform.w = width;
    platform.h = height;
    platform.event(RT_EVENT_WINDOW_RESIZE, kj_cast(f32, width), kj_cast(f32, height), 0.0f);
}

static void glfw_mouse_pos_callback(GLFWwindow* window, f64 x, f64 y) {
    kj_unused(window);
    platform.event(RT_EVENT_MOUSE_MOVE, kj_cast(f32, x), kj_cast(f32, y), 0.0f);
}

static void glfw_mouse_button_callback(GLFWwindow* window, i32 button, i32 action, i32 mods) {
    kj_unused(window);
    platform.event(RT_EVENT_MOUSE_EVENT, kj_cast(f32, button), kj_cast(f32, action), kj_cast(f32, mods));
}

void rt_set_init(rtInitFn fn) {
    platform.init = fn;
}

void rt_set_event(rtEventFn fn) {
    platform.event = fn;
}

void rt_set_update(rtUpdateFn fn) {
    platform.update = fn;
}

void rt_set_render(rtRenderFn fn) {
    platform.render = fn;
}

void rt_set_exit(rtExitFn fn) {
    platform.exit = fn;
}

f32 rt_get_time() {
    return kj_cast(f32, glfwGetTime());
}

rtAction rt_get_key(rtKey key) {
    return kj_cast(rtAction, glfwGetKey(platform.window, kj_cast(i32, key)));
}

void rt_loop() {
    f32 new_time = rt_get_time();
    f32 frame_time = new_time - platform.curr_time;
    platform.curr_time = new_time;
    platform.acc_time += frame_time;
    while(platform.acc_time >= platform.timestep){
        glfwPollEvents();
        platform.update(platform.time, platform.timestep);
        platform.acc_time -= platform.timestep;
        platform.time += platform.timestep;
    }
    platform.render();
}

i32 rt_run(i32 width, i32 height, f32 timestep) {
    i32 res = 0;
    glfwSetErrorCallback(glfw_err_callback);
    if(glfwInit()) {
        glfwWindowHint(GLFW_RESIZABLE, KJ_FALSE);
        glfwWindowHint(GLFW_DOUBLEBUFFER, KJ_TRUE);
        glfwWindowHint(GLFW_SRGB_CAPABLE, KJ_TRUE);
        glfwWindowHint(GLFW_SAMPLES, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

        platform.w = width;
        platform.h = height;
        platform.window = glfwCreateWindow(platform.w, platform.h, "roytracer", NULL, NULL);
        if(platform.window) {
            glfwMakeContextCurrent(platform.window);

            if(gladLoadGL()) {
                glfwSwapInterval(1);
                glfwSetKeyCallback(platform.window, glfw_key_callback);
                glfwSetWindowSizeCallback(platform.window, glfw_window_size_callback);
                glfwSetCursorPosCallback(platform.window, glfw_mouse_pos_callback);
                glfwSetMouseButtonCallback(platform.window, glfw_mouse_button_callback);

                platform.timestep = timestep;
                platform.time = 0.0f;
                platform.curr_time = rt_get_time();
                if(platform.init) {
                    platform.init();
                }

                while(!glfwWindowShouldClose(platform.window)) {
                    rt_loop();
                    glfwSwapBuffers(platform.window);
                }

                if(platform.exit) {
                    platform.exit();
                }
                glfwMakeContextCurrent(0);
                glfwTerminate();
            } else {
                glfwTerminate();
                kj_printf("unable to initialise GL\n");
                res = -1;
            }
        } else {
            glfwTerminate();
            kj_printf("unable to initialise window\n");
            res = -1;
        }
    } else {
        kj_printf("unable to initialise platform layer\n");
        res = -1;
    }
    return res;
}

static rtState state = {0};

RT_INIT(game_init) {
    state.w = RT_WINDOW_WIDTH;
    state.h = RT_WINDOW_HEIGHT;

    glViewport(0, 0, state.w, state.h);

    raytrace_init(state.w, state.h);
}

RT_EVENT(game_event) {
    kj_unused(d3);
    if(e == RT_EVENT_WINDOW_RESIZE) {
        state.w = kj_cast(i32, d1);
        state.h = kj_cast(i32, d2);
        raytrace_resize(state.w, state.h);
    }
}

RT_UPDATE(game_update) {
    state.t = t;
    state.dt = dt;
}

RT_RENDER(game_render) {
    raytrace_render(state.w, state.h, state.t, state.dt, 80.0f);
}

RT_EXIT(game_exit) {
    raytrace_destroy();
}

i32 main(void) {
    rt_set_init(game_init);
    rt_set_event(game_event);
    rt_set_update(game_update);
    rt_set_render(game_render);
    rt_set_exit(game_exit);
    return rt_run(RT_WINDOW_WIDTH, RT_WINDOW_HEIGHT, RT_TIMESTEP);
}

#define KJ_IMPL
#include <kj/kj.h>
#define KJ_MATH_IMPL
#include <kj/kj_math.h>
