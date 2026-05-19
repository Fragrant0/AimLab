#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

#include "game_manager.h"

GameManager* g_GameManager = nullptr;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(GameManager::SCR_WIDTH, GameManager::SCR_HEIGHT, "FPS Game", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    g_GameManager = new GameManager();
    if (!g_GameManager->Initialize(window))
    {
        std::cout << "Failed to initialize GameManager" << std::endl;
        delete g_GameManager;
        glfwTerminate();
        return -1;
    }

    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        g_GameManager->ProcessInput(window);
        g_GameManager->Update(deltaTime);
        g_GameManager->Render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    delete g_GameManager;
    g_GameManager = nullptr;

    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    if (g_GameManager)
        g_GameManager->ProcessFramebufferSize(width, height);
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
