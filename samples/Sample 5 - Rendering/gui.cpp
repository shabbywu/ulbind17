#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
// clang-format on

#include <codecvt>

#include <Ultralight/Ultralight.h>
#include <ulbind17/ulbind17.hpp>

#include "BitmapRender.hpp"

using namespace ultralight;

static void framebuffer_size_callback(GLFWwindow *window, int width, int height);
static void processInput(GLFWwindow *window);

static void cursor_position_callback(GLFWwindow *window, double xpos, double ypos);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 450;

int showGUI(RefPtr<Renderer> &renderer, RefPtr<View> &view) {
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DECORATED, GL_FALSE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetWindowUserPointer(window, view.get());

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    glfwSetWindowPos(window, (mode->width - SCR_WIDTH) / 2, (mode->height - SCR_HEIGHT) / 2);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // OpenGL state
    // ------------
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    render::bitmap::BitmapRender r;
    while (!glfwWindowShouldClose(window)) {
        // input
        // -----
        processInput(window);
        renderer->Update();

        // render
        // ------
        renderer->RefreshDisplay(0);
        renderer->Render();
        BitmapSurface *surface = (BitmapSurface *)(view->surface());
        if (!surface->dirty_bounds().IsEmpty()) {
            r.Update(surface->bitmap());
            surface->ClearDirtyBounds();
        }
        r.Render(0, 0, SCR_WIDTH, SCR_HEIGHT);

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
static void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        auto view = (ultralight::View *)glfwGetWindowUserPointer(window);
        double xpos;
        double ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        MouseEvent evt;
        evt.type = MouseEvent::kType_MouseDown;
        evt.x = xpos;
        evt.y = ypos;
        evt.button = MouseEvent::kButton_Left;
        view->FireMouseEvent(evt);
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) {
        auto view = (ultralight::View *)glfwGetWindowUserPointer(window);
        double xpos;
        double ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        MouseEvent evt;
        evt.type = MouseEvent::kType_MouseUp;
        evt.x = xpos;
        evt.y = ypos;
        evt.button = MouseEvent::kButton_Left;
        view->FireMouseEvent(evt);
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
static void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

static void cursor_position_callback(GLFWwindow *window, double xpos, double ypos) {
    auto view = (ultralight::View *)glfwGetWindowUserPointer(window);
    MouseEvent evt;
    evt.type = MouseEvent::kType_MouseMoved;
    evt.x = xpos;
    evt.y = ypos;
    evt.button = MouseEvent::kButton_None;
    view->FireMouseEvent(evt);
}
