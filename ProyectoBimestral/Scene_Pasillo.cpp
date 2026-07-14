#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>
#include <learnopengl/text_renderer.h>
#include <iostream>

extern enum SceneType { SCENE_PASILLO, SCENE_SAMY, SCENE_ANI, SCENE_MATTHEW, SCENE_JOSUE };
extern SceneType g_CurrentScene;
extern SceneType g_NextScene;

namespace Pasillo {
    static unsigned int SCR_WIDTH = 1920;
    static unsigned int SCR_HEIGHT = 1080;

    // Posición inicial dentro del pasillo (X en [0, 35], Z en [-4.3, -0.4])
    static Camera camera(glm::vec3(3.0f, 1.2f, -2.3f));
    static float lastX = 1920.0f / 2.0f;
    static float lastY = 1080.0f / 2.0f;
    static bool firstMouse = true;

    static float deltaTime = 0.0f;
    static float lastFrame = 0.0f;

    static bool flashlightOn = true;
    static bool fKeyPressedLastFrame = false;

    void framebuffer_size_callback(GLFWwindow* window, int width, int height)
    {
        glViewport(0, 0, width, height);
        SCR_WIDTH = width;
        SCR_HEIGHT = height;
    }

    void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
    {
        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);

        if (firstMouse)
        {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos;

        lastX = xpos;
        lastY = ypos;

        camera.ProcessMouseMovement(xoffset, yoffset);
    }

    void processInput(GLFWwindow* window)
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Movimiento con WASD
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.ProcessKeyboard(RIGHT, deltaTime);

        // Control de linterna
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
        {
            if (!fKeyPressedLastFrame)
            {
                flashlightOn = !flashlightOn;
                fKeyPressedLastFrame = true;
            }
        }
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_RELEASE)
        {
            fKeyPressedLastFrame = false;
        }

        // Teclas para viajar a otras escenas (recuerdos) sin conflicto con WASD
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS)
            g_NextScene = SCENE_SAMY;
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS)
            g_NextScene = SCENE_ANI;
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS)
            g_NextScene = SCENE_MATTHEW;
        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)
            g_NextScene = SCENE_JOSUE;
    }

    void RunScene(GLFWwindow* window)
    {
        // Configurar callbacks del window
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // Obtener tamaño actual de la ventana
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        SCR_WIDTH = width;
        SCR_HEIGHT = height;

        // Shaders
        Shader ourShader("shaders/Vertex_Samy.vs", "shaders/Fragment_Samy.fs");
        Shader textShader("shaders/text_samy.vs", "shaders/text_samy.fs");

        // Cargar modelo del pasillo
        Model pasilloModel("./model/pasillo/pasillo.obj");

        // Cargar fuentes
        InitFreeType("fonts/arial.ttf");

        // Reiniciar estado de cámara apuntando por el pasillo (Yaw = 0.0f)
        camera = Camera(glm::vec3(3.0f, 1.2f, -2.3f), glm::vec3(0.0f, 1.0f, 0.0f), 0.0f, 0.0f);
        firstMouse = true;
        lastFrame = static_cast<float>(glfwGetTime());

        glEnable(GL_DEPTH_TEST);

        while (!glfwWindowShouldClose(window) && g_NextScene == g_CurrentScene)
        {
            float currentFrame = static_cast<float>(glfwGetTime());
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            processInput(window);

            glClearColor(0.02f, 0.02f, 0.02f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            ourShader.use();

            // Parámetros de la cámara
            ourShader.setVec3("viewPos", camera.Position);

            // Dirección de luz tenue
            ourShader.setVec3("dirLight.direction", glm::vec3(-0.2f, -1.0f, -0.3f));
            ourShader.setVec3("dirLight.ambient", glm::vec3(0.05f, 0.05f, 0.05f));
            ourShader.setVec3("dirLight.diffuse", glm::vec3(0.1f, 0.1f, 0.12f));
            ourShader.setVec3("dirLight.specular", glm::vec3(0.1f, 0.1f, 0.12f));

            // Luz SpotLight (linterna del jugador)
            ourShader.setVec3("spotLight.position", camera.Position);
            ourShader.setVec3("spotLight.direction", camera.Front);
            ourShader.setFloat("spotLight.constant", 1.0f);
            ourShader.setFloat("spotLight.linear", 0.09f);
            ourShader.setFloat("spotLight.quadratic", 0.032f);
            ourShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
            ourShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(17.5f)));

            if (flashlightOn)
            {
                ourShader.setVec3("spotLight.ambient", glm::vec3(0.0f));
                ourShader.setVec3("spotLight.diffuse", glm::vec3(2.0f, 2.0f, 1.8f));
                ourShader.setVec3("spotLight.specular", glm::vec3(1.2f, 1.2f, 1.0f));
            }
            else
            {
                ourShader.setVec3("spotLight.ambient", glm::vec3(0.0f));
                ourShader.setVec3("spotLight.diffuse", glm::vec3(0.0f));
                ourShader.setVec3("spotLight.specular", glm::vec3(0.0f));
            }

            // Proyecciones
            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
            glm::mat4 view = camera.GetViewMatrix();
            ourShader.setMat4("projection", projection);
            ourShader.setMat4("view", view);

            // Dibujar el modelo del pasillo
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
            model = glm::scale(model, glm::vec3(1.0f));
            ourShader.setMat4("model", model);
            pasilloModel.Draw(ourShader);

            // Dibujar HUD
            float sw = static_cast<float>(SCR_WIDTH);
            float sh = static_cast<float>(SCR_HEIGHT);
            RenderText(textShader, "EL PABELLON 9 - RECUERDO SEPULTADO", 50.0f, sh - 60.0f, 0.6f, glm::vec3(0.8f, 0.8f, 0.9f), sw, sh);
            RenderText(textShader, "Muevase con WASD. Presione F para Linterna.", 50.0f, sh - 100.0f, 0.45f, glm::vec3(0.6f, 0.6f, 0.6f), sw, sh);
            RenderText(textShader, "Seleccione una puerta (recuerdo):", 50.0f, 120.0f, 0.5f, glm::vec3(0.9f, 0.8f, 0.8f), sw, sh);
            RenderText(textShader, "1 o G -> Garage | 2 o I -> Infancia | 3 o T -> Navidad | 4 o J -> Final", 50.0f, 60.0f, 0.6f, glm::vec3(0.9f, 0.2f, 0.2f), sw, sh);

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }
}
