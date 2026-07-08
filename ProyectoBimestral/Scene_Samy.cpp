// ==============================
//   ESCENA Samy 
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
#include <cstdlib>

#define STB_IMAGE_IMPLEMENTATION
#include <learnopengl/stb_image.h>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void ClampPlayerToGarage();
float HorrorFlicker(float time);

// settings
const unsigned int SCR_WIDTH = 2560;
const unsigned int SCR_HEIGHT = 1600;

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
// EVENTOS Y TRASLADOS 
// -------------------------------------------------------------------------------------
bool crimeSceneRender = true;
bool horrorLightOn = false;

glm::vec3 crimeScenePos = glm::vec3(-0.05f, -0.228f, 0.60f);
glm::vec3 bodyOriginalPos = glm::vec3(-0.08f, -0.22f, 0.50f);
glm::vec3 horrorLightPos = glm::vec3(-0.11f, -0.25f, -0.90f);
glm::vec3 llantasPos = glm::vec3(0.08f, -0.19f, -0.9f);
glm::vec3 monsterPos = glm::vec3(0.00f, -0.228f, -0.5f);

// AJUSTES DE ESCALA Y ROTACIÓN GENERAL
float bodyScale = 0.018f;
float crimeSceneScale = 0.082f;
float bodyRotationY = -45.0f;
float llantasScale = 0.1f;
float monsterSpeed = 0.18f;
float monsterScale = 0.12f;

// =====================================================================================
// *** PANEL DE CONTROL DE LA LINTERNA 
// =====================================================================================
float linternaScale = 0.0001f;   // Reducido drásticamente. Si sigue enorme, prueba con 0.0005f
float linternaOffsetX = 0.015f;    // Izquierda (-) / Derecha (+) respecto a tus ojos
float linternaOffsetY = -0.085f;   // Abajo (-) / Arriba (+) respecto a tus ojos
float linternaOffsetZ = 0.18f;    // Atrás (-) / Adelante (+) para alejarlo de la pantalla

// Rotaciones extra por si el modelo vino girado por defecto en su archivo .obj
float linternaFixRotX = 0.0f;
float linternaFixRotY = 0.0f;
float linternaFixRotZ = 0.0f;
// =====================================================================================

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

    // ---- CARGA DE MODELOS ----
    Model garageModel("./model/garage/garage.obj");
    Model crimeSceneModel("./model/crime_scene/crime_scene.obj");
    Model bodyModel("./model/body/body.obj");
    Model llantasModel("./model/llantas/llantas.obj");
    Model monsterModel("./model/monster/monster.obj");
    Model linternaModel("./model/linterna/linterna.obj");

    camera.MovementSpeed = debugMode ? 0.5f : 0.2f;

    int frameCount = 0;
    float previousTime = (float)glfwGetTime();

    // render loop
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // --- CÁLCULO DE FPS ---
        frameCount++;
        if (currentFrame - previousTime >= 1.0f)
        {
            std::string title = "Garage F1 - Horror Scene | FPS: " + std::to_string(frameCount);
            glfwSetWindowTitle(window, title.c_str());
            frameCount = 0;
            previousTime = currentFrame;
        }

        timeElapsed += deltaTime;
        processInput(window);

        if (timeElapsed > 25.0f) horrorLightOn = true;

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

        if (horrorLightOn) {
            float horrorFlicker = HorrorFlicker(currentFrame * 2.5f);
            glm::vec3 horrorLightColor = glm::vec3(0.7f, 0.05f, 0.05f);
            ourShader.setVec3("pointLights[1].position", horrorLightPos);
            ourShader.setVec3("pointLights[1].ambient", horrorLightColor * 0.01f * horrorFlicker);
            ourShader.setVec3("pointLights[1].diffuse", horrorLightColor * horrorFlicker);
            ourShader.setVec3("pointLights[1].specular", horrorLightColor * horrorFlicker);
        }
        else {
            ourShader.setVec3("pointLights[1].diffuse", glm::vec3(0.0f));
            ourShader.setVec3("pointLights[1].specular", glm::vec3(0.0f));
        }
        ourShader.setFloat("pointLights[1].constant", 1.0f);
        ourShader.setFloat("pointLights[1].linear", 0.14f);
        ourShader.setFloat("pointLights[1].quadratic", 0.07f);

        // Apagar luces extra si las hay
        for (int i = 2; i < 4; i++) {
            std::string base = "pointLights[" + std::to_string(i) + "].";
            ourShader.setVec3(base + "position", glm::vec3(0.0f));
            ourShader.setVec3(base + "ambient", glm::vec3(0.0f));
            ourShader.setVec3(base + "diffuse", glm::vec3(0.0f));
            ourShader.setVec3(base + "specular", glm::vec3(0.0f));
            ourShader.setFloat(base + "constant", 1.0f);
            ourShader.setFloat(base + "linear", 0.09f);
            ourShader.setFloat(base + "quadratic", 0.032f);
        }

        // =========================================================================
        // LÓGICA DE LA LINTERNA 
        // =========================================================================
        glm::vec3 handOffset = (camera.Right * linternaOffsetX) + (camera.Up * linternaOffsetY) + (camera.Front * linternaOffsetZ);
        glm::vec3 currentLinternaPos = camera.Position + handOffset;

        ourShader.setVec3("spotLight.position", currentLinternaPos);
        ourShader.setVec3("spotLight.direction", camera.Front);

        if (flashlightOn) {
            ourShader.setVec3("spotLight.ambient", glm::vec3(0.0f));
            ourShader.setVec3("spotLight.diffuse", glm::vec3(1.0f, 1.0f, 0.9f));
            ourShader.setVec3("spotLight.specular", glm::vec3(1.0f, 1.0f, 0.9f));
        }
        else {
            ourShader.setVec3("spotLight.ambient", glm::vec3(0.0f));
            ourShader.setVec3("spotLight.diffuse", glm::vec3(0.0f));
            ourShader.setVec3("spotLight.specular", glm::vec3(0.0f));
        }
        ourShader.setFloat("spotLight.constant", 1.0f);
        ourShader.setFloat("spotLight.linear", 0.09f);
        ourShader.setFloat("spotLight.quadratic", 0.032f);
        ourShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
        ourShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(17.5f)));

        // =========================================================================
        // DIBUJADO DE MODELOS
        // =========================================================================

        // 1) EL GARAGE 
        glm::mat4 garageMat = glm::mat4(1.0f);
        garageMat = glm::translate(garageMat, glm::vec3(0.0f, 0.0f, 0.0f));
        garageMat = glm::scale(garageMat, glm::vec3(garageScale));
        ourShader.setMat4("model", garageMat);
        garageModel.Draw(ourShader);

        // 2) MODELO CRIME SCENE
        if (crimeSceneRender) {
            glm::mat4 crimeSceneMat = glm::mat4(1.0f);
            crimeSceneMat = glm::translate(crimeSceneMat, crimeScenePos);
            crimeSceneMat = glm::scale(crimeSceneMat, glm::vec3(crimeSceneScale));
            ourShader.setMat4("model", crimeSceneMat);
            crimeSceneModel.Draw(ourShader);
        }

        // 3) MODELO BODY 
        glm::mat4 bodyMat = glm::mat4(1.0f);
        bodyMat = glm::translate(bodyMat, bodyOriginalPos);
        bodyMat = glm::rotate(bodyMat, glm::radians(bodyRotationY), glm::vec3(0.0f, 1.0f, 0.0f));
        bodyMat = glm::scale(bodyMat, glm::vec3(bodyScale));
        ourShader.setMat4("model", bodyMat);
        bodyModel.Draw(ourShader);

        // 4) MODELO LLANTAS
        glm::mat4 llantasMat = glm::mat4(1.0f);
        llantasMat = glm::translate(llantasMat, llantasPos);
        llantasMat = glm::scale(llantasMat, glm::vec3(llantasScale));
        ourShader.setMat4("model", llantasMat);
        llantasModel.Draw(ourShader);

        // 5) MODELO MONSTRUO
        glm::vec3 direction = camera.Position - monsterPos;
        direction.y = 0.0f;

        if (glm::length(direction) > 0.05f)
        {
            direction = glm::normalize(direction);
            monsterPos += direction * monsterSpeed * deltaTime;
        }

        float monsterRotationY = glm::degrees(glm::atan(direction.x, direction.z));

        glm::mat4 monsterMat = glm::mat4(1.0f);
        monsterMat = glm::translate(monsterMat, monsterPos);
        monsterMat = glm::rotate(monsterMat, glm::radians(monsterRotationY), glm::vec3(0.0f, 1.0f, 0.0f));
        monsterMat = glm::scale(monsterMat, glm::vec3(monsterScale));
        ourShader.setMat4("model", monsterMat);
        monsterModel.Draw(ourShader);

        // 6) MODELO DE LA LINTERNA EN LA MANO
        if (flashlightOn) {
            glm::mat4 linternaMat = glm::mat4(1.0f);
            linternaMat = glm::translate(linternaMat, currentLinternaPos);

            // Rotación automática para seguir la mirada de la cámara
            linternaMat = glm::rotate(linternaMat, glm::radians(-camera.Yaw - 90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            linternaMat = glm::rotate(linternaMat, glm::radians(camera.Pitch), glm::vec3(1.0f, 0.0f, 0.0f));

            // CORRECCIONES MANUALES DE ROTACIÓN (usa las variables de arriba si el modelo apunta mal)
            linternaMat = glm::rotate(linternaMat, glm::radians(linternaFixRotX), glm::vec3(1.0f, 0.0f, 0.0f));
            linternaMat = glm::rotate(linternaMat, glm::radians(linternaFixRotY), glm::vec3(0.0f, 1.0f, 0.0f));
            linternaMat = glm::rotate(linternaMat, glm::radians(linternaFixRotZ), glm::vec3(0.0f, 0.0f, 1.0f));

            // Escala definitiva de la linterna
            linternaMat = glm::scale(linternaMat, glm::vec3(linternaScale));

            ourShader.setMat4("model", linternaMat);
            linternaModel.Draw(ourShader);
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
    float cycleTime = glm::mod(time, 180.0f);

    if (cycleTime < 0.6f)
    {
        float flicker = glm::sin(cycleTime * 60.0f);
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

    // --- LÓGICA DE INTERACCIÓN DE LA LINTERNA (TOGGLE LIMPIO) ---
    bool fPressed = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    if (fPressed && !fKeyPressedLastFrame)
    {
        flashlightOn = !flashlightOn;
    }
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