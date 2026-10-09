// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
// clang-format on

#include <Ultralight/Ultralight.h>
#include "BitmapRender.hpp"
#include "../common.hpp"
#include <chrono>
#include <iostream>
#include <vector>

using namespace ultralight;

static void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    glViewport(0, 0, width, height);
    int logical_width, logical_height;
    glfwGetWindowSize(window, &logical_width, &logical_height);
    if (logical_width > 0 && logical_height > 0)
        static_cast<View *>(glfwGetWindowUserPointer(window))->Resize(logical_width, logical_height);
}

static void cursor_position_callback(GLFWwindow *window, double x, double y) {
    MouseEvent event;
    event.type = MouseEvent::kType_MouseMoved;
    event.x = static_cast<int>(x);
    event.y = static_cast<int>(y);
    event.button = MouseEvent::kButton_None;
    static_cast<View *>(glfwGetWindowUserPointer(window))->FireMouseEvent(event);
}

static void mouse_button_callback(GLFWwindow *window, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT)
        return;
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    MouseEvent event;
    event.type = action == GLFW_PRESS ? MouseEvent::kType_MouseDown : MouseEvent::kType_MouseUp;
    event.x = static_cast<int>(x);
    event.y = static_cast<int>(y);
    event.button = MouseEvent::kButton_Left;
    static_cast<View *>(glfwGetWindowUserPointer(window))->FireMouseEvent(event);
}

int showGUI(RefPtr<Renderer> &renderer, RefPtr<View> &view, bool smoke) {
    if (!glfwInit())
        return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (smoke)
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow *window = glfwCreateWindow(800, 450, "ulbind17 / Ultralight 2.0", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    struct Cleanup {
        GLFWwindow *window;
        ~Cleanup() { glfwDestroyWindow(window); glfwTerminate(); }
    } cleanup {window};
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetWindowUserPointer(window, view.get());
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        return -1;
    }
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    framebuffer_size_callback(window, width, height);
    // OpenGL resources must be destroyed before their context.
    {
        render::bitmap::BitmapRender bitmap;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(window, true);
            renderer->RefreshDisplay(0);
            renderer->Update();
            renderer->Render();
            auto *surface = static_cast<BitmapSurface *>(view->surface());
            if (surface && !surface->dirty_bounds().IsEmpty()) {
                bitmap.Update(surface->bitmap());
                surface->ClearDirtyBounds();
            }
            glClearColor(1, 1, 1, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            bitmap.Render(0, 0, view->width(), view->height());
            if (smoke) {
                sample::check(std::chrono::steady_clock::now() < deadline, "rendering smoke test timed out");
                ulbind17::js::Context context(view.get());
                auto count = context.Evaluate<int>("document.querySelectorAll('button').length");
                if (count && count.value() == 5) {
                    auto border = sample::take(context.Evaluate<std::vector<double>>(
                        "(() => {const r=document.querySelector('.button3').getBoundingClientRect();"
                        "return [r.left+1,r.top+r.height/2]})()"));
                    unsigned char pixel[4] = {};
                    int framebuffer_width, framebuffer_height;
                    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
                    glReadPixels(static_cast<int>(border[0] * framebuffer_width / view->width()),
                                 framebuffer_height - 1 - static_cast<int>(border[1] * framebuffer_height / view->height()),
                                 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
                    sample::check(pixel[0] > 180 && pixel[1] < 150 && pixel[2] < 150,
                                  "CPU bitmap colors or row stride were uploaded incorrectly");
                    auto point = sample::take(context.Evaluate<std::vector<double>>(
                        "(() => {const r=document.querySelector('.button1').getBoundingClientRect();"
                        "return [r.left+r.width/2,r.top+r.height/2]})()"));
                    MouseEvent event;
                    event.x = static_cast<int>(point[0]);
                    event.y = static_cast<int>(point[1]);
                    event.button = MouseEvent::kButton_Left;
                    event.type = MouseEvent::kType_MouseDown;
                    view->FireMouseEvent(event);
                    event.type = MouseEvent::kType_MouseUp;
                    view->FireMouseEvent(event);
                    renderer->Update();
                    glfwSetWindowShouldClose(window, true);
                }
            }
            glfwSwapBuffers(window);
        }
    }
    return 0;
}
