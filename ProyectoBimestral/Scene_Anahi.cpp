#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "learnopengl/stb_image.h" 

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void processInput(GLFWwindow* window);
float HorrorFlicker(float time);

// Dimensiones de ventana
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// =====================================================================================
// POSICIÓN INICIAL DE LA CÁMARA (APUNTANDO AL CENTRO INTERIOR)
// =====================================================================================
Camera camera(glm::vec3(0.0f, 1.2f, 4.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Tiempos
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Estado de la linterna
bool flashlightOn = true;
bool fKeyPressedLastFrame = false;

// Configuración de iluminación ambiental básica de fondo
glm::vec3 colorLuzTecho = glm::vec3(0.8f, 0.8f, 0.7f);

// =====================================================================================
// VARIABLES GLOBALES - ESCALA CALIBRADA PARA EL NUEVO FANTASMA
// =====================================================================================
float ghostScale = 350.0f;

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Backroom Garage Scene - Anahi", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    stbi_set_flip_vertically_on_load(true);

    Shader ourShader("shaders/Vertex_Anahi.vs", "shaders/Fragment_Anahi.fs");

    // =====================================================================================
    // CARGA DE MODELOS 
    // =====================================================================================
    Model garageModel("./model/garage/garage.obj");
    Model ghostModel("./model/fantasma/fantasma.obj");

    camera.MovementSpeed = 2.5f;

    // Render Loop
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // Fondo oscuro lúgubre
        glClearColor(0.03f, 0.03f, 0.03f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();
        ourShader.setVec3("viewPos", camera.Position);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        float intensidadFlicker = HorrorFlicker(currentFrame);

        // Foco Central (PointLight)
        ourShader.setVec3("pointLight.position", glm::vec3(0.0f, 4.0f, 0.0f));
        ourShader.setFloat("pointLight.constant", 1.0f);
        ourShader.setFloat("pointLight.linear", 0.045f);
        ourShader.setFloat("pointLight.quadratic", 0.0075f);
        ourShader.setVec3("pointLight.ambient", colorLuzTecho * 0.15f * intensidadFlicker);
        ourShader.setVec3("pointLight.diffuse", colorLuzTecho * 2.2f * intensidadFlicker);
        ourShader.setVec3("pointLight.specular", colorLuzTecho * 2.2f * intensidadFlicker);

        // Linterna Interactiva (SpotLight)
        ourShader.setVec3("spotLight.position", camera.Position);
        ourShader.setVec3("spotLight.direction", camera.Front);
        ourShader.setFloat("spotLight.constant", 1.0f);
        ourShader.setFloat("spotLight.linear", 0.045f);
        ourShader.setFloat("spotLight.quadratic", 0.016f);
        ourShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(15.0f)));
        ourShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(22.0f)));

        if (flashlightOn)
        {
            ourShader.setVec3("spotLight.ambient", glm::vec3(0.0f));
            ourShader.setVec3("spotLight.diffuse", glm::vec3(2.5f, 2.5f, 2.2f));
            ourShader.setVec3("spotLight.specular", glm::vec3(2.5f, 2.5f, 2.2f));
        }
        else
        {
            ourShader.setVec3("spotLight.ambient", glm::vec3(0.0f));
            ourShader.setVec3("spotLight.diffuse", glm::vec3(0.0f));
            ourShader.setVec3("spotLight.specular", glm::vec3(0.0f));
        }

        // -----------------------------------------------------------------------------
        // DIBUJAR ESCENARIO: EL GARAJE
        // -----------------------------------------------------------------------------
        glm::mat4 modelMat = glm::mat4(1.0f);
        modelMat = glm::translate(modelMat, glm::vec3(0.0f, 0.0f, 0.0f));
        ourShader.setMat4("model", modelMat);
        garageModel.Draw(ourShader);

        // -----------------------------------------------------------------------------
        // DIBUJAR ENTIDAD: EL FANTASMA EN EL CENTRO EXACTO (CORREGIDO EN EJES)
        // -----------------------------------------------------------------------------
        glm::mat4 ghostMat = glm::mat4(20.0f);

        // 1. Posicionamiento en el centro geométrico del garaje (Un poco elevado)
        glm::vec3 centroEscena = glm::vec3(0.0f, 0.7f, 0.0f);
        ghostMat = glm::translate(ghostMat, centroEscena);

        // 2. Rotación automática en Y para que encare la orientación de la cámara
        glm::vec3 targetDir = glm::normalize(camera.Position - centroEscena);
        float angle = atan2(targetDir.x, targetDir.z);
        ghostMat = glm::rotate(ghostMat, angle, glm::vec3(0.0f, 1.0f, 0.0f));

        // 3. CORRECCIÓN DE EJES BLENDER: Rotación de 90 grados en X para levantarlo
        ghostMat = glm::rotate(ghostMat, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

        // 4. Aplicamos el escalado masivo requerido para este .obj específico
        ghostMat = glm::scale(ghostMat, glm::vec3(ghostScale));

        ourShader.setMat4("model", ghostMat);
        ghostModel.Draw(ourShader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

// Control dinámico del parpadeo de las luces
float HorrorFlicker(float time)
{
    float baseIntensity = 0.85f;
    float cycleTime = fmod(time, 8.0f);

    if (cycleTime > 2.0f && cycleTime < 2.3f)
    {
        return (sin(time * 50.0f) > 0.0f) ? 0.1f : baseIntensity;
    }
    if (cycleTime > 5.5f && cycleTime < 5.7f)
    {
        return 0.0f;
    }
    return baseIntensity;
}

// Controles de movimiento básicos de la cámara
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    bool fPressed = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    if (fPressed && !fKeyPressedLastFrame)
        flashlightOn = !flashlightOn;
    fKeyPressedLastFrame = fPressed;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;

    lastX = (float)xpos;
    lastY = (float)ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}