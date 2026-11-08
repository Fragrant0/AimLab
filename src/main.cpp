#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <exception>
#include <fstream>
#include <memory>
#include <iostream>
#include <string>
#include <vector>

#include "game_manager.h"

GameManager* g_GameManager = nullptr;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

namespace
{
    bool g_WindowCallbackFailed = false;

    bool SaveCapture(const std::string& path, int width, int height)
    {
        if (width <= 0 || height <= 0)
            return false;

        std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
        GLint previousAlignment = 4;
        glGetIntegerv(GL_PACK_ALIGNMENT, &previousAlignment);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        glPixelStorei(GL_PACK_ALIGNMENT, previousAlignment);
        bool hasErrors = false;
        for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError())
        {
            std::cerr << "[Capture] OpenGL error: " << error << std::endl;
            hasErrors = true;
        }
        if (hasErrors)
            return false;

        std::ofstream output(path, std::ios::binary);
        if (!output)
        {
            std::cerr << "[Capture] Cannot open output: " << path << std::endl;
            return false;
        }
        output << "P6\n" << width << " " << height << "\n255\n";
        const size_t rowBytes = static_cast<size_t>(width) * 3;
        for (int row = height - 1; row >= 0; --row)
            output.write(reinterpret_cast<const char*>(pixels.data() + row * rowBytes), rowBytes);
        output.close();
        if (!output)
        {
            std::cerr << "[Capture] Failed to write output: " << path << std::endl;
            return false;
        }
        std::cout << "[Capture] Saved " << path << std::endl;
        return true;
    }
}

int main(int argc, char** argv)
{
    bool captureMode = false;
    bool captureDebugOverlay = false;
    int captureMapIndex = 0;
    std::string capturePath;
    if (argc > 1)
    {
        if ((argc != 4 && argc != 5) || std::string(argv[1]) != "--capture" ||
            (std::string(argv[2]) != "0" && std::string(argv[2]) != "1" && std::string(argv[2]) != "2") ||
            std::string(argv[3]).empty() || (argc == 5 && std::string(argv[4]) != "--debug-overlay"))
        {
            std::cerr << "Usage: opengl_try.exe [--capture <0|1|2> <output.ppm> [--debug-overlay]]" << std::endl;
            return 2;
        }
        captureMode = true;
        captureMapIndex = argv[2][0] - '0';
        capturePath = argv[3];
        captureDebugOverlay = argc == 5;
    }

    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (captureMode)
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    int windowWidth = static_cast<int>(GameManager::SCR_WIDTH);
    int windowHeight = static_cast<int>(GameManager::SCR_HEIGHT);
    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* videoMode = primaryMonitor ? glfwGetVideoMode(primaryMonitor) : nullptr;
    if (videoMode && !captureMode)
    {
        windowWidth = std::min(windowWidth, std::max(640, videoMode->width - 120));
        windowHeight = std::min(windowHeight, std::max(360, videoMode->height - 120));
        windowHeight = std::min(windowHeight, static_cast<int>(windowWidth * 9.0f / 16.0f));
        windowWidth = static_cast<int>(windowHeight * 16.0f / 9.0f);
    }

    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "AimLab | OpenGL Rendering Lab", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    if (videoMode && !captureMode)
    {
        glfwSetWindowPos(window,
                         videoMode->width / 2 - windowWidth / 2,
                         videoMode->height / 2 - windowHeight / 2);
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    if (!captureMode)
    {
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetScrollCallback(window, scroll_callback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    int exitCode = 1;
    std::unique_ptr<GameManager> gameManager;
    try
    {
        gameManager = std::make_unique<GameManager>();
        g_GameManager = gameManager.get();
        if (!gameManager->Initialize(window))
        {
            std::cerr << "[FATAL] Game manager initialization failed." << std::endl;
        }
        else if (captureMode)
        {
            bool captured = false;
            if (gameManager->PrepareShowcase(captureMapIndex, captureDebugOverlay))
            {
                constexpr int captureFrames = 30;
                constexpr float captureDeltaTime = 1.0f / 60.0f;
                double previousFrame = glfwGetTime();
                for (int frame = 0; frame < captureFrames; ++frame)
                {
                    const double currentFrame = glfwGetTime();
                    gameManager->Update(captureDeltaTime, static_cast<float>(currentFrame - previousFrame));
                    previousFrame = currentFrame;
                    gameManager->Render();
                    if (frame + 1 < captureFrames)
                        glfwSwapBuffers(window);
                }
                int width = 0;
                int height = 0;
                glfwGetFramebufferSize(window, &width, &height);
                captured = !g_WindowCallbackFailed && SaveCapture(capturePath, width, height);
            }
            else
            {
                std::cerr << "[Capture] Could not prepare requested map." << std::endl;
            }
            exitCode = captured ? 0 : 1;
        }
        else
        {
            double lastFrame = glfwGetTime();
            while (!glfwWindowShouldClose(window))
            {
                int framebufferWidth = 0;
                int framebufferHeight = 0;
                glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
                if (framebufferWidth <= 0 || framebufferHeight <= 0 || glfwGetWindowAttrib(window, GLFW_ICONIFIED))
                {
                    glfwWaitEvents();
                    lastFrame = glfwGetTime();
                    continue;
                }

                const double currentFrame = glfwGetTime();
                const float frameTime = static_cast<float>(currentFrame - lastFrame);
                const float deltaTime = std::clamp(frameTime, 0.0f, 0.1f);
                lastFrame = currentFrame;

                gameManager->ProcessInput(window, deltaTime);
                gameManager->Update(deltaTime, frameTime);
                gameManager->Render();

                glfwSwapBuffers(window);
                glfwPollEvents();
            }
            exitCode = g_WindowCallbackFailed ? 1 : 0;
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "[FATAL] Runtime failure: " << error.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "[FATAL] Unknown runtime failure." << std::endl;
    }

    // GL resources must be released before their context is destroyed.
    g_GameManager = nullptr;
    gameManager.reset();
    glfwDestroyWindow(window);
    glfwTerminate();
    return exitCode;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    try
    {
        if (g_GameManager)
            g_GameManager->ProcessFramebufferSize(width, height);
    }
    catch (const std::exception& error)
    {
        // Do not unwind a C++ exception through GLFW's native callback boundary.
        std::cerr << "[FATAL] Window resize failed: " << error.what() << std::endl;
        g_WindowCallbackFailed = true;
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    catch (...)
    {
        std::cerr << "[FATAL] Unknown window resize failure." << std::endl;
        g_WindowCallbackFailed = true;
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    if (g_GameManager)
        g_GameManager->ProcessMouseMovement(static_cast<float>(xposIn), static_cast<float>(yposIn));
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (g_GameManager)
        g_GameManager->ProcessMouseScroll(static_cast<float>(yoffset));
}
