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
#include <random> // Para el efecto de temblor inquietante

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
// CONFIGURACIÓN DE CÁMARA Y DINÁMICAS DEL JUGADOR
// =====================================================================================
Camera camera(glm::vec3(0.0f, 1.2f, 4.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Variables para el cabeceo de la cámara (Head Bobbing)
float bobbingTimer = 0.0f;
bool isMoving = false;

// Tiempos
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Estado de la linterna
bool flashlightOn = true;
bool fKeyPressedLastFrame = false;

// Configuración de iluminación ambiental
glm::vec3 colorLuzTecho = glm::vec3(0.75f, 0.75f, 0.7f);

// =====================================================================================
// VARIABLES GLOBALES - COORDENADAS ESTÁTICAS DEL PAYASO
// =====================================================================================
glm::vec3 clownPosition = glm::vec3(0.0f, 0.0f, -2.0f);
float clownScale = 0.0038f;
const float CLOWN_FEET_OFFSET = 8.926056f;

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

    // Carga de modelos
    Model garageModel("./model/garage/garage.obj");
    Model clownModel("./model/payaso/payaso.obj");

    camera.MovementSpeed = 2.5f;

    // Configuración para el generador de temblores lúgubres
    std::default_random_engine generator;
    std::uniform_real_distribution<float> distribution(-0.008f, 0.008f);

    // Render Loop
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // Lógica de Head Bobbing (cabeceo al caminar)
        if (isMoving)
        {
            bobbingTimer += deltaTime * 12.0f;
            camera.Position.y = 1.2f + sin(bobbingTimer) * 0.05f;
        }
        else
        {
            bobbingTimer = 0.0f;
            camera.Position.y = 1.2f;
        }

        // Fondo oscuro
        glClearColor(0.02f, 0.02f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();
        ourShader.setVec3("viewPos", camera.Position);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        float intensidadFlicker = HorrorFlicker(currentFrame);

        // -----------------------------------------------------------------------------
        // 🌟 LÓGICA DE VIDEOJUEGO: DETECTOR DE JUMPSCARE (DISTANCIA TRIDIMENSIONAL)
        // -----------------------------------------------------------------------------
        // Medimos la distancia real entre la cámara y la posición base del payaso
        float distanciaAlPayaso = glm::distance(camera.Position, clownPosition);

        float dynamicClownScale = clownScale;
        bool jumpscareActivo = false;

        if (distanciaAlPayaso < 1.1f) // Si te acercas demasiado (Zonas de peligro)
        {
            jumpscareActivo = true;

            // 1. Bloqueamos la mirada: Forzamos el Front de la cámara para que apunte directo a sus ojos
            camera.Front = glm::normalize(clownPosition - camera.Position);

            // 2. Modificamos la luz: Hacemos un parpadeo caótico, histérico y ultra rápido
            intensidadFlicker = (sin(currentFrame * 90.0f) > 0.0f) ? 0.0f : 4.0f;

            // 3. Modificamos el tamaño: El payaso se infla de golpe para saltar agresivamente a tu cara
            dynamicClownScale = clownScale * 1.7f;
        }

        // PointLight (Foco Fijo del Techo)
        ourShader.setVec3("pointLight.position", glm::vec3(0.0f, 4.0f, 0.0f));
        ourShader.setFloat("pointLight.constant", 1.0f);
        ourShader.setFloat("pointLight.linear", 0.07f);
        ourShader.setFloat("pointLight.quadratic", 0.017f);
        ourShader.setVec3("pointLight.ambient", colorLuzTecho * 0.12f * intensidadFlicker);
        ourShader.setVec3("pointLight.diffuse", colorLuzTecho * 0.9f * intensidadFlicker);
        ourShader.setVec3("pointLight.specular", colorLuzTecho * 0.9f * intensidadFlicker);

        // SpotLight (Linterna con desfase sutil por respiración)
        glm::vec3 dynamicLinternaPos = camera.Position;
        if (isMoving) {
            dynamicLinternaPos.x += cos(bobbingTimer) * 0.02f;
        }

        ourShader.setVec3("spotLight.position", dynamicLinternaPos);
        ourShader.setVec3("spotLight.direction", camera.Front);
        ourShader.setFloat("spotLight.constant", 1.0f);
        ourShader.setFloat("spotLight.linear", 0.045f);
        ourShader.setFloat("spotLight.quadratic", 0.012f);
        ourShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(14.5f)));
        ourShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(20.0f)));

        if (flashlightOn)
        {
            // Si el jumpscare está activo, la linterna se descontrola y brilla con un tono rojo de alarma
            if (jumpscareActivo) {
                ourShader.setVec3("spotLight.diffuse", glm::vec3(3.0f, 0.0f, 0.0f));
                ourShader.setVec3("spotLight.specular", glm::vec3(3.0f, 0.0f, 0.0f));
            }
            else {
                ourShader.setVec3("spotLight.diffuse", glm::vec3(2.2f, 2.2f, 1.9f));
                ourShader.setVec3("spotLight.specular", glm::vec3(2.2f, 2.2f, 1.9f));
            }
            ourShader.setVec3("spotLight.ambient", glm::vec3(0.0f));
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
        // DIBUJAR ENTIDAD: EL PAYASO CON SISTEMA DE SUSTO INTEGRADO
        // -----------------------------------------------------------------------------
        glm::mat4 clownMat = glm::mat4(1.0f);

        glm::vec3 clownFinalPos = clownPosition;
        clownFinalPos.y += CLOWN_FEET_OFFSET * clownScale;

        // Efecto flotación constante (levitación espectral)
        clownFinalPos.y += sin(currentFrame * 2.0f) * 0.08f;

        // Micro-temblores aleatorios. Si te asusta, el temblor se duplica por el shock
        float factorTemblor = jumpscareActivo ? 3.0f : 1.0f;
        clownFinalPos.x += distribution(generator) * factorTemblor;
        clownFinalPos.z += distribution(generator) * factorTemblor;

        clownMat = glm::translate(clownMat, clownFinalPos);

        // El payaso busca tu posición con la mirada dinámicamente
        glm::vec3 targetDir = glm::normalize(camera.Position - clownPosition);
        float angle = atan2(targetDir.x, targetDir.z);

        const float CLOWN_ROTATION_OFFSET = glm::radians(-50.0f);
        angle += CLOWN_ROTATION_OFFSET;

        clownMat = glm::rotate(clownMat, angle, glm::vec3(0.0f, 1.0f, 0.0f));

        // Escalado dinámico (Normal en reposo / Masivo en Jumpscare)
        clownMat = glm::scale(clownMat, glm::vec3(dynamicClownScale));

        ourShader.setMat4("model", clownMat);
        clownModel.Draw(ourShader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

float HorrorFlicker(float time)
{
    float baseIntensity = 0.85f;
    float cycleTime = fmod(time, 8.0f);

    if (cycleTime > 2.0f && cycleTime < 2.3f)
    {
        return (sin(time * 50.0f) > 0.0f) ? 0.15f : baseIntensity;
    }
    if (cycleTime > 5.5f && cycleTime < 5.7f)
    {
        return 0.05f;
    }
    return baseIntensity;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    isMoving = false;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        camera.ProcessKeyboard(FORWARD, deltaTime);
        isMoving = true;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        camera.ProcessKeyboard(BACKWARD, deltaTime);
        isMoving = true;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        camera.ProcessKeyboard(LEFT, deltaTime);
        isMoving = true;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        camera.ProcessKeyboard(RIGHT, deltaTime);
        isMoving = true;
    }

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