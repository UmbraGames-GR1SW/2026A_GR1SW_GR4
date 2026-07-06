// =====================================================================================
//   ESCENA Samy 
// =====================================================================================

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

// --- SFML Audio (Comentado por el momento) ---
// #include <SFML/Audio.hpp>

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
// EVENTOS Y TRASLADOS 
// -------------------------------------------------------------------------------------
bool crimeSceneRender = true;
bool bodyMoved = false;
bool horrorLightOn = false;

// 
glm::vec3 crimeScenePos = glm::vec3(-0.05f, -0.228f, 0.60f);

// :
glm::vec3 bodyOriginalPos = glm::vec3(-0.08f, -0.22f, 0.50f);

//
glm::vec3 horrorLightPos = glm::vec3(-0.11f, -0.25f, -0.90f);

glm::vec3 bodyCurrentPos = bodyOriginalPos;

// AJUSTES DE ESCALA Y ROTACIÓN
float bodyScale = 0.018f;         // Reducido ligeramente si aún se veía grande
float crimeSceneScale = 0.082f;   // MODIFICADO: Disminuido el tamaño de la escena del crimen (antes 0.2f)
float bodyRotationY = -45.0f;    // MODIFICADO: Rotación de 45 grados a la derecha (sentido horario)

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

    camera.MovementSpeed = debugMode ? 0.5f : 0.2f;

    // render loop
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        timeElapsed += deltaTime;

        processInput(window);

        // Activar la luz de suspenso a los 25 segundos
        if (timeElapsed > 25.0f) {
            horrorLightOn = true;
        }

        // -----------------------------------------------------------------------------
        // LÓGICA DE TRASLADO DEL CUERPO (BODY) AL ACERCARSE A LA COORDENADA 3
        // -----------------------------------------------------------------------------
        if (horrorLightOn && !bodyMoved) {
            float distanceToLight = glm::distance(camera.Position, horrorLightPos);

            // Si el jugador se aproxima a menos de 0.5 unidades de la coordenada 3
            if (distanceToLight < 0.5f) {
                bodyCurrentPos = horrorLightPos; // El cuerpo se traslada inmediatamente
                bodyMoved = true;
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

        // Luz 0: Luz parpadeante por defecto en el garage
        ourShader.setVec3("pointLights[0].position", flickerLightPos);
        ourShader.setVec3("pointLights[0].ambient", flickerLightColor * 0.02f * flickerIntensity);
        ourShader.setVec3("pointLights[0].diffuse", flickerLightColor * flickerIntensity);
        ourShader.setVec3("pointLights[0].specular", flickerLightColor * flickerIntensity);
        ourShader.setFloat("pointLights[0].constant", 1.0f);
        ourShader.setFloat("pointLights[0].linear", 0.09f);
        ourShader.setFloat("pointLights[0].quadratic", 0.032f);

        // Luz 1: Luz lúgubre en la coordenada 3 (Roja y parpadeante a los 25 segundos)
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

        // Desactivamos el resto de las luces del arreglo (2 y 3)
        for (int i = 2; i < 4; i++)
        {
            std::string base = "pointLights[" + std::to_string(i) + "].";
            ourShader.setVec3(base + "position", glm::vec3(0.0f));
            ourShader.setVec3(base + "ambient", glm::vec3(0.0f));
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

        // 2) MODELO CRIME SCENE
        if (crimeSceneRender) {
            glm::mat4 crimeSceneMat = glm::mat4(1.0f);
            crimeSceneMat = glm::translate(crimeSceneMat, crimeScenePos);
            crimeSceneMat = glm::scale(crimeSceneMat, glm::vec3(crimeSceneScale)); // Escala disminuida
            ourShader.setMat4("model", crimeSceneMat);
            crimeSceneModel.Draw(ourShader);
        }

        // 3) MODELO BODY (Con ajustes de posición, escala disminuida y rotación)
        glm::mat4 bodyMat = glm::mat4(1.0f);
        bodyMat = glm::translate(bodyMat, bodyCurrentPos);
        bodyMat = glm::rotate(bodyMat, glm::radians(bodyRotationY), glm::vec3(0.0f, 1.0f, 0.0f)); // Rotación aplicada
        bodyMat = glm::scale(bodyMat, glm::vec3(bodyScale));
        ourShader.setMat4("model", bodyMat);
        bodyModel.Draw(ourShader);

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