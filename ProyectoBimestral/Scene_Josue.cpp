// ==============================
//    ESCENA VR ROOM - Josue
// ==============================
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include <learnopengl/stb_image.h>
#include <filesystem>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void ClampPlayerToRoom();

bool pKeyPressedLastFrame = false;

// settings
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// =========================================================
// ESCALA Y POSICIÓN
// =========================================================
float worldScale = 0.05f;
Camera camera(glm::vec3(0.0f, 1.2f, 1.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float gameStartTime = 0.0f; // NUEVO: Temporizador para controlar el inicio del nivel

// --- Entidad que persigue al jugador ---
glm::vec3 entityPos = glm::vec3(0.0f, 0.0f, -25.0f);
float entitySpeed = 2.0f;
bool entityInitialized = false;

// =========================================================
// VARIABLES DE LA LINTERNA 
// =========================================================
bool flashlightOn = true;
bool fKeyPressedLastFrame = false;
float linternaOffsetX = 0.015f;
float linternaOffsetY = -0.085f;
float linternaOffsetZ = 0.18f;

// =========================================================
// LÍMITES DE COLISIÓN (Paredes ajustadas para 4 pasillos)
// =========================================================
float roomMinX = -1.8f;
float roomMaxX = 2.1f;
float roomMinZ = -18.0f;
float roomMaxZ = 347.0f;
float playerHeight = 1.2f;
// =========================================================

// =========================================================
// COORDENADAS BASE DE LOS LETREROS (Del Piso)
// =========================================================
float baseLightX[5] = { 0.10074f,   0.259016f,  0.0407524f, 0.0422577f, 0.113721f };
float baseLightZ[5] = { -0.387055f, 17.4112f,   32.5504f,   50.2641f,   65.4025f };
float roofHeightY = 4.0f;

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "VR Room - Josue", NULL, NULL);
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

    glEnable(GL_DEPTH_TEST);

    std::cout << "Working directory: " << std::filesystem::current_path() << std::endl;

    Shader ourShader("shaders/Vertex_Josue.vs", "shaders/Fragment_Josue.fs");

    Model exitModel("./model/exit/exit.obj");
    Model entidadModel("./model/exit/entidad.obj");

    // Guardamos el tiempo en el que inicia la partida
    gameStartTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        float absoluteTime = (float)glfwGetTime();
        deltaTime = absoluteTime - lastFrame;
        lastFrame = absoluteTime;

        // Calculamos cuánto tiempo ha pasado en ESTA vida (se resetea al morir)
        float currentSceneTime = absoluteTime - gameStartTime;

        processInput(window);

        // =========================================================
        // VELOCIDAD DE LA ENTIDAD (LENTA LOS PRIMEROS 7 SEGUNDOS)
        // =========================================================
        if (currentSceneTime < 7.0f) {
            entitySpeed = 2.0f;
        }
        else {
            entitySpeed = 25.0f;
        }

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 500.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);
        ourShader.setVec3("viewPos", camera.Position);

        ourShader.setVec3("lightDir", glm::vec3(-0.3f, -1.0f, -0.2f));
        ourShader.setVec3("lightColor", glm::vec3(0.02f, 0.02f, 0.03f));
        ourShader.setFloat("specularStrength", 0.05f);

        glm::vec3 handOffset = (camera.Right * linternaOffsetX) + (camera.Up * linternaOffsetY) + (camera.Front * linternaOffsetZ);
        glm::vec3 currentLinternaPos = camera.Position + handOffset;

        ourShader.setVec3("spotLight.position", currentLinternaPos);
        ourShader.setVec3("spotLight.direction", camera.Front);

        if (flashlightOn) {
            ourShader.setVec3("spotLight.ambient", glm::vec3(0.0f));
            ourShader.setVec3("spotLight.diffuse", glm::vec3(2.0f, 2.0f, 1.8f));
            ourShader.setVec3("spotLight.specular", glm::vec3(1.2f, 1.2f, 1.0f));
        }
        else {
            ourShader.setVec3("spotLight.ambient", glm::vec3(0.0f));
            ourShader.setVec3("spotLight.diffuse", glm::vec3(0.0f));
            ourShader.setVec3("spotLight.specular", glm::vec3(0.0f));
        }

        ourShader.setFloat("spotLight.constant", 1.0f);
        ourShader.setFloat("spotLight.linear", 0.09f);
        ourShader.setFloat("spotLight.quadratic", 0.032f);
        ourShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(10.0f)));
        ourShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(25.0f)));

        int numPasillos = 4;
        float longitudPasillo = 91.5f;
        int lightIndex = 0;

        for (int i = 0; i < numPasillos; i++)
        {
            glm::mat4 exitMat = glm::mat4(1.0f);
            exitMat = glm::translate(exitMat, glm::vec3(0.0f, 1.0f * worldScale, i * longitudPasillo));
            exitMat = glm::scale(exitMat, glm::vec3(worldScale));
            ourShader.setMat4("model", exitMat);
            exitModel.Draw(ourShader);

            for (int j = 0; j < 5; j++)
            {
                float currentZ = baseLightZ[j] + (i * longitudPasillo);
                glm::vec3 pos = glm::vec3(baseLightX[j], roofHeightY, currentZ);

                float phase = (float)lightIndex * 2.3f;
                // Usamos absoluteTime para que las luces no den un salto visual al respawnear
                float noise = sin(absoluteTime * 12.0f + phase) * cos(absoluteTime * 15.0f + phase * 0.5f);
                float flicker = (noise > 0.4f) ? 0.15f : 1.0f;

                std::string base = "pointLights[" + std::to_string(lightIndex) + "].";
                ourShader.setVec3(base + "position", pos);

                ourShader.setVec3(base + "ambient", glm::vec3(0.05f, 0.0f, 0.0f) * flicker);
                ourShader.setVec3(base + "diffuse", glm::vec3(1.5f, 0.1f, 0.1f) * flicker);
                ourShader.setVec3(base + "specular", glm::vec3(1.0f, 0.0f, 0.0f) * flicker);

                ourShader.setFloat(base + "constant", 1.0f);
                ourShader.setFloat(base + "linear", 0.3f);
                ourShader.setFloat(base + "quadratic", 0.2f);

                lightIndex++;
            }
        }

        // =========================================================
        // MOVIMIENTO DE LA ENTIDAD EN LÍNEA RECTA
        // =========================================================
        glm::vec3 direction = glm::vec3(0.0f, 0.0f, camera.Position.z - entityPos.z);
        float distZ = glm::length(direction);

        if (distZ > 0.05f)
        {
            direction = glm::normalize(direction);
            entityPos += direction * entitySpeed * deltaTime;
        }

        float angleToPlayer = atan2(direction.x, direction.z);

        glm::mat4 entidadMat = glm::mat4(1.0f);
        entidadMat = glm::translate(entidadMat, entityPos);
        entidadMat = glm::rotate(entidadMat, angleToPlayer, glm::vec3(0.0f, 1.0f, 0.0f));
        entidadMat = glm::scale(entidadMat, glm::vec3(worldScale));
        ourShader.setMat4("model", entidadMat);
        entidadModel.Draw(ourShader);

        // =========================================================
        // COLISIÓN Y RESPAWN (GAME OVER)
        // =======================================================
        // Calculamos la distancia solo en el piso (ejes X y Z), ignorando la altura Y de tu cámara
        float dx = camera.Position.x - entityPos.x;
        float dz = camera.Position.z - entityPos.z;
        float captureDistance = sqrt((dx * dx) + (dz * dz));

        float collisionRadius = 2.0f; // Radio más amplio y justo

        if (captureDistance < collisionRadius)
        {
            // Reiniciar posición del jugador
            camera.Position = glm::vec3(0.0f, 1.2f, 1.0f);

            // Reiniciar posición de la entidad
            entityPos = glm::vec3(0.0f, 0.0f, -25.0f);

            // Reiniciar el temporizador para que la entidad vuelva a ir lenta 7 segundos
            gameStartTime = (float)glfwGetTime();
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void ClampPlayerToRoom()
{
    camera.Position.x = glm::clamp(camera.Position.x, roomMinX, roomMaxX);
    camera.Position.z = glm::clamp(camera.Position.z, roomMinZ, roomMaxZ);
    camera.Position.y = playerHeight;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // =========================================================
    // LÓGICA DE SPRINT CON SHIFT IZQUIERDO
    // =========================================================
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camera.MovementSpeed = 25.4f;
    else
        camera.MovementSpeed = 8.0f; // Velocidad normal lenta

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    ClampPlayerToRoom();

    bool fPressed = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    if (fPressed && !fKeyPressedLastFrame)
    {
        flashlightOn = !flashlightOn;
    }
    fKeyPressedLastFrame = fPressed;

    bool pPressed = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
    if (pPressed && !pKeyPressedLastFrame)
    {
        std::cout << "camera.Position = ("
            << camera.Position.x << ", "
            << camera.Position.y << ", "
            << camera.Position.z << ")" << std::endl;
    }
    pKeyPressedLastFrame = pPressed;
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

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll((float)yoffset);
}