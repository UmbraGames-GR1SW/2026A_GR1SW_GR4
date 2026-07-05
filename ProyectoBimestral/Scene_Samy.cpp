// =====================================================================================
//  ESCENA Samy 
// =====================================================================================

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

// --- SFML Audio ---
#include <SFML/Audio.hpp>

#include <iostream>
#include <string>
#include <cstdlib>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include <learnopengl/stb_image.h>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void ClampPlayerToGarage();
float HorrorFlicker(float time);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// -------------------------------------------------------------------------------------
// PERSONAJE / CAMARA
// -------------------------------------------------------------------------------------
Camera camera(glm::vec3(0.695734f, -0.132227f, 0.389627f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing y eventos
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float timeElapsed = 0.0f;

// -------------------------------------------------------------------------------------
// ESCENA / LUCES
// -------------------------------------------------------------------------------------
float garageScale = 1.0f;

glm::vec3 flickerLightPos = glm::vec3(5.0f, 3.0f, 2.0f);
glm::vec3 flickerLightColor = glm::vec3(1.0f, 1.0f, 0.95f);
glm::vec3 ambientLightColor = glm::vec3(0.04f, 0.04f, 0.045f);

// -------------------------------------------------------------------------------------
// COLISIONES / LIMITES DEL GARAGE 
// -------------------------------------------------------------------------------------
float garageMinX = -0.744853f;
float garageMaxX = 0.695734f;
float garageMinZ = -1.03401f;
float garageMaxZ = 1.00179f;
float playerHeight = -0.1375f;

bool flashlightOn = true;
bool fKeyPressedLastFrame = false;
bool debugMode = false;
bool pKeyPressedLastFrame = false;

// -------------------------------------------------------------------------------------
// EVENTOS Y MODELO PERSEGUIDOR
// -------------------------------------------------------------------------------------
bool f1SoundPlayed = false;
bool fearSoundPlayed = false;
bool horrorSoundPlayed = false;
bool renderDemon = false;

glm::vec3 demonPosition = glm::vec3(0.0f, playerHeight, -1.0f); // Inicia en el fondo
float demonScale = 0.05f;
float demonSpeed = 0.2f;

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Garage F1 - Horror Scene", NULL, NULL);
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

    Shader ourShader("shaders/Vertex_Samy.vs", "shaders/Fragment_Samy.fs");

    // ---- CARGA DE MODELOS (Auto eliminado) ----
    Model garageModel("./model/garage/garage.obj");
    Model demonModel("./model/demon/demon.obj"); // Nuevo modelo

    // ---- CARGA DE AUDIOS SFML ----
    // ---- CARGA DE AUDIOS SFML ----
    sf::Music f1Audio;
    if (!f1Audio.openFromFile("radio.mp3")) {
        std::cout << "Error audio F1\n";
    }

    sf::Music fearAudio;
    // CAmbiado a .mp3 según lo que me indicas
    if (!fearAudio.openFromFile("auto.mp3")) {
        std::cout << "Error audio Fear\n";
    }

    sf::Music horrorAudio;
    // Cambiado a .mp3
    if (!horrorAudio.openFromFile("suspenso.mp3")) {
        std::cout << "Error audio Horror\n";
    } else {
    horrorAudio.setLooping(true);
    }

    camera.MovementSpeed = debugMode ? 0.5f : 0.2f;

    // render loop
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        timeElapsed += deltaTime; // Actualizar cronómetro

        processInput(window);

        // -----------------------------------------------------------------------------
        // SECUENCIA DE TIEMPOS
        // -----------------------------------------------------------------------------
        if (timeElapsed > 2.0f && !f1SoundPlayed) {
            f1Audio.play();
            f1SoundPlayed = true;
        }

        if (timeElapsed > 10.0f && !fearSoundPlayed) {
            fearAudio.play();
            fearSoundPlayed = true;
        }

        if (timeElapsed > 18.0f && !horrorSoundPlayed) {
            horrorAudio.play();
            horrorSoundPlayed = true;
            renderDemon = true; // El monstruo aparece
        }

        // -----------------------------------------------------------------------------
        // LÓGICA DE PERSECUCIÓN
        // -----------------------------------------------------------------------------
        if (renderDemon) {
            glm::vec3 directionToPlayer = camera.Position - demonPosition;
            directionToPlayer.y = 0.0f; // Mantenerlo al ras del suelo

            float distance = glm::length(directionToPlayer);
            if (distance > 0.3f) {
                directionToPlayer = glm::normalize(directionToPlayer);
                demonPosition += directionToPlayer * demonSpeed * deltaTime;
            }
        }

        float time = currentFrame;
        float flickerIntensity = HorrorFlicker(time);

        if (debugMode)
            glClearColor(0.5f, 0.55f, 0.6f, 1.0f);
        else
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();
        ourShader.setVec3("viewPos", camera.Position);
        ourShader.setBool("debugFullBright", debugMode);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 500.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        ourShader.setVec3("dirLight.direction", glm::vec3(-0.2f, -1.0f, -0.3f));
        ourShader.setVec3("dirLight.ambient", ambientLightColor);
        ourShader.setVec3("dirLight.diffuse", glm::vec3(0.0f));
        ourShader.setVec3("dirLight.specular", glm::vec3(0.0f));

        ourShader.setVec3("pointLights[0].position", flickerLightPos);
        ourShader.setVec3("pointLights[0].ambient", flickerLightColor * 0.02f * flickerIntensity);
        ourShader.setVec3("pointLights[0].diffuse", flickerLightColor * flickerIntensity);
        ourShader.setVec3("pointLights[0].specular", flickerLightColor * flickerIntensity);
        ourShader.setFloat("pointLights[0].constant", 1.0f);
        ourShader.setFloat("pointLights[0].linear", 0.09f);
        ourShader.setFloat("pointLights[0].quadratic", 0.032f);

        for (int i = 1; i < 4; i++)
        {
            std::string base = "pointLights[" + std::to_string(i) + "].";
            ourShader.setVec3(base + "position", glm::vec3(0.0f));
            ourShader.setVec3(base + "ambient", glm::vec3(0.01f));
            ourShader.setVec3(base + "diffuse", glm::vec3(0.0f));
            ourShader.setVec3(base + "specular", glm::vec3(0.0f));
            ourShader.setFloat(base + "constant", 1.0f);
            ourShader.setFloat(base + "linear", 0.09f);
            ourShader.setFloat(base + "quadratic", 0.032f);
        }

        ourShader.setVec3("spotLight.position", camera.Position);
        ourShader.setVec3("spotLight.direction", camera.Front);
        if (flashlightOn)
        {
            ourShader.setVec3("spotLight.ambient", glm::vec3(0.0f));
            ourShader.setVec3("spotLight.diffuse", glm::vec3(1.0f, 1.0f, 0.9f));
            ourShader.setVec3("spotLight.specular", glm::vec3(1.0f, 1.0f, 0.9f));
        }
        else
        {
            ourShader.setVec3("spotLight.ambient", glm::vec3(0.0f));
            ourShader.setVec3("spotLight.diffuse", glm::vec3(0.0f));
            ourShader.setVec3("spotLight.specular", glm::vec3(0.0f));
        }
        ourShader.setFloat("spotLight.constant", 1.0f);
        ourShader.setFloat("spotLight.linear", 0.09f);
        ourShader.setFloat("spotLight.quadratic", 0.032f);
        ourShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
        ourShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(17.5f)));

        // 1) EL GARAGE 
        glm::mat4 garageMat = glm::mat4(1.0f);
        garageMat = glm::translate(garageMat, glm::vec3(0.0f, 0.0f, 0.0f));
        garageMat = glm::scale(garageMat, glm::vec3(garageScale));
        ourShader.setMat4("model", garageMat);
        garageModel.Draw(ourShader);

        // 2) EL DEMONIO (Se dibuja hacia el jugador)
        if (renderDemon) {
            glm::mat4 demonMat = glm::mat4(1.0f);
            demonMat = glm::translate(demonMat, demonPosition);

            // Rotación dinámica hacia el jugador
            glm::vec3 direction = glm::normalize(camera.Position - demonPosition);
            float angle = atan2(direction.x, direction.z);
            demonMat = glm::rotate(demonMat, angle, glm::vec3(0.0f, 1.0f, 0.0f));

            demonMat = glm::scale(demonMat, glm::vec3(demonScale));
            ourShader.setMat4("model", demonMat);
            demonModel.Draw(ourShader);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void ClampPlayerToGarage()
{
    camera.Position.x = glm::clamp(camera.Position.x, garageMinX, garageMaxX);
    camera.Position.z = glm::clamp(camera.Position.z, garageMinZ, garageMaxZ);
    camera.Position.y = playerHeight;
}

float HorrorFlicker(float time)
{
    float baseIntensity = 0.35f;
    float cycleTime = fmod(time, 180.0f);

    if (cycleTime < 0.6f)
    {
        float flicker = sin(cycleTime * 60.0f);
        return (flicker > 0.0f) ? baseIntensity * 0.15f : baseIntensity;
    }

    return baseIntensity;
}

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

    if (debugMode)
    {
        float flySpeed = camera.MovementSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            camera.Position.y += flySpeed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
            camera.Position.y -= flySpeed;

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
    else
    {
        ClampPlayerToGarage();
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

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll((float)yoffset);
}