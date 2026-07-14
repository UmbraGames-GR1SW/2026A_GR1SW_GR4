#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <learnopengl/stb_image.h>

enum SceneType { SCENE_PASILLO, SCENE_SAMY, SCENE_ANI, SCENE_MATTHEW, SCENE_JOSUE };
SceneType g_CurrentScene = SCENE_PASILLO;
SceneType g_NextScene = SCENE_PASILLO;

namespace Pasillo { void RunScene(GLFWwindow* window); }
namespace Samy { void RunScene(GLFWwindow* window); }
namespace Anahi { void RunScene(GLFWwindow* window); }
namespace Warehouse { void RunScene(GLFWwindow* window); }
namespace Josue { void RunScene(GLFWwindow* window); }

int main()
{
    // Inicializar GLFW
    if (!glfwInit())
    {
        std::cout << "Error al inicializar GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Crear la ventana principal de la simulación
    GLFWwindow* window = glfwCreateWindow(1280, 720, "El Pabellon 9 - Simulador de Recuerdos", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Error al crear la ventana GLFW" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Inicializar GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Error al inicializar GLAD" << std::endl;
        return -1;
    }

    // Bucle enrutador principal
    while (!glfwWindowShouldClose(window))
    {
        g_CurrentScene = g_NextScene;

        if (g_CurrentScene == SCENE_PASILLO)
        {
            Pasillo::RunScene(window);
        }
        else if (g_CurrentScene == SCENE_SAMY)
        {
            Samy::RunScene(window);
        }
        else if (g_CurrentScene == SCENE_ANI)
        {
            Anahi::RunScene(window);
        }
        else if (g_CurrentScene == SCENE_MATTHEW)
        {
            Warehouse::RunScene(window);
        }
        else if (g_CurrentScene == SCENE_JOSUE)
        {
            Josue::RunScene(window);
        }
    }

    glfwTerminate();
    return 0;
}