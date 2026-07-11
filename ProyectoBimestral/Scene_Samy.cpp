// ==============================
//    ESCENA Samy 
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
#include <learnopengl/audio.h>
#include <learnopengl/text_renderer.h>

#define STB_IMAGE_IMPLEMENTATION
#include <learnopengl/stb_image.h>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void ClampPlayerToGarage();
float HorrorFlicker(float time);

// settings
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

// -------------------------------------------------------------------------------------
// PERSONAJE / CAMARA
// -------------------------------------------------------------------------------------
Camera camera(glm::vec3(0.69f, 0.88f, 0.38f));
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
bool cKeyPressedLastFrame = false;
bool showMenu = true;
bool mKeyPressedLastFrame = false;
bool upKeyPressedLastFrame = false;
bool downKeyPressedLastFrame = false;

bool backupFlashlightState = true;

// -------------------------------------------------------------------------------------
// EVENTOS Y TRASLADOS 
// -------------------------------------------------------------------------------------
bool crimeSceneRender = true;
bool horrorLightOn = false;

glm::vec3 crimeScenePos = glm::vec3(-0.05f, -0.228f, 0.60f);
glm::vec3 bodyOriginalPos = glm::vec3(-0.08f, -0.22f, 0.50f);
glm::vec3 llantasPos = glm::vec3(0.08f, -0.19f, -0.9f);
glm::vec3 monsterPos = glm::vec3(0.00f, 50.0f, -0.5f);
glm::vec3 phonePos = glm::vec3(-0.4f, 0.03f, -0.9f);

// AJUSTES DE ESCALA Y ROTACIÓN GENERAL
float bodyScale = 0.018f;
float crimeSceneScale = 0.082f;
float bodyRotationY = -45.0f;
float llantasScale = 0.1f;
float monsterScale = 0.07f;
float phoneScale = 0.0016f;

// =====================================================================================
// *** PANEL DE CONTROL DEL MONSTRUO Y EVENTOS DE INTERACCIÓN
// =====================================================================================
bool jumpscareActive = false;
bool jumpscareFinished = false;
float jumpscareTimer = 0.0f;

float monsterHeightOffset = -0.205f;
float monsterDistanceOffset = 0.20f;

// Variables de control de audio y secuencias
bool fondoPlayed = false;
bool phoneRingPlayed = false;
bool phoneCallPlayed = false;
bool nearPhone = false;
bool callAnswered = false;

float phoneCallTimer = 0.0f;

float maxPhoneCallTime = 6.0f;       // Duración total de la luz roja en el teléfono tras contestar
float timeBeforeJumpscare = 10.0f;    // El monstruo saldrá exactamente a los 10 segundos de iniciada la llamada

// Control de la luz roja focalizada sobre el teléfono
bool phoneRedLightOn = false;

// Variables obsoletas
float text3DScale = 0.12f;
glm::vec3 textPosOffset = glm::vec3(0.0f, 0.15f, 0.0f);

// =====================================================================================
// *** PANEL DE CONTROL DE LA LINTERNA 
// =====================================================================================
float linternaScale = 0.0001f;
float linternaOffsetX = 0.015f;
float linternaOffsetY = -0.085f;
float linternaOffsetZ = 0.18f;

float linternaFixRotX = 0.0f;
float linternaFixRotY = 0.0f;
float linternaFixRotZ = 0.0f;

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Garage ", NULL, NULL);
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

    initaudio();

    InitFreeType("fonts/arial.ttf");

    Shader ourShader("shaders/Vertex_Samy.vs", "shaders/Fragment_Samy.fs");
    Shader textShader("shaders/text_samy.vs", "shaders/text_samy.fs");

    // ---- CARGA DE MODELOS ----
    Model garageModel("./model/garage/garage.obj");
    Model crimeSceneModel("./model/crime_scene/crime_scene.obj");
    Model bodyModel("./model/body/body.obj");
    Model llantasModel("./model/llantas/llantas.obj");
    Model monsterModel("./model/monstruo/Monstruo.obj");
    Model linternaModel("./model/linterna/linterna.obj");
    Model phoneModel("./model/phone/phone.obj");
    camera.MovementSpeed = debugMode ? 0.5f : 0.2f;

    int frameCount = 0;
    float previousTime = (float)glfwGetTime();
    lastFrame = (float)glfwGetTime();

    // render loop
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        if (deltaTime > 0.2f) deltaTime = 0.2f;

        frameCount++;
        if (currentFrame - previousTime >= 1.0f)
        {
            std::string title = "Garage | FPS: " + std::to_string(frameCount);
            glfwSetWindowTitle(window, title.c_str());
            frameCount = 0;
            previousTime = currentFrame;
        }

        timeElapsed += deltaTime;
        processInput(window);

        // 1. Audio de fondo
        if (!fondoPlayed && timeElapsed >= 0.0f)
        {
            playsound("music/Fondo.mp3", 0);
            fondoPlayed = true;
        }

        // 2. Phone ring en bucle a los 15s 
        if (!phoneRingPlayed && timeElapsed >= 15.0f)
        {
            haltmusic();
            playsound("music/phone ring.mp3", 1);
            phoneRingPlayed = true;
            phoneRedLightOn = true;
        }

        // 3. Control de tiempos post-interacción (Tecla C presionada)
        if (callAnswered && !jumpscareFinished)
        {
            phoneCallTimer += deltaTime;

            if (phoneCallTimer >= maxPhoneCallTime)
            {
                phoneRedLightOn = false;
            }

            if (!jumpscareActive && phoneCallTimer >= timeBeforeJumpscare)
            {
                jumpscareActive = true;
                horrorLightOn = true;
                jumpscareTimer = 0.0f;

                backupFlashlightState = flashlightOn;
                flashlightOn = false;

                haltmusic();
                playsound("music/suspenso.mp3", 0);
            }
        }

        if (jumpscareActive)
        {
            jumpscareTimer += deltaTime;
            monsterPos = camera.Position + (camera.Front * monsterDistanceOffset) + (camera.Up * monsterHeightOffset);

            if (jumpscareTimer >= 1.2f)
            {
                jumpscareActive = false;
                jumpscareFinished = true;
                horrorLightOn = false;
                monsterPos = glm::vec3(0.00f, -50.0f, -0.5f);
                flashlightOn = backupFlashlightState;
                haltmusic();
            }
        }

        float distanceToPhone = glm::distance(camera.Position, phonePos);
        nearPhone = (distanceToPhone < 0.35f);

        // ================================
        // CONTROL DEL FONDO 
        // ================================
        float time = currentFrame;
        float flickerIntensity = HorrorFlicker(time);

        if (jumpscareActive)
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        else if (debugMode)
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

        // --- LUZ DIRECCIONAL / AMBIENTAL ---
        ourShader.setVec3("dirLight.direction", glm::vec3(-0.2f, -1.0f, -0.3f));
        if (jumpscareActive) {
            ourShader.setVec3("dirLight.ambient", glm::vec3(0.0f));
        }
        else {
            ourShader.setVec3("dirLight.ambient", ambientLightColor);
        }
        ourShader.setVec3("dirLight.diffuse", glm::vec3(0.0f));
        ourShader.setVec3("dirLight.specular", glm::vec3(0.0f));

        // --- PUNTO DE LUZ 0 (Techo del Garage) ---
        ourShader.setVec3("pointLights[0].position", flickerLightPos);
        if (jumpscareActive) {
            ourShader.setVec3("pointLights[0].ambient", glm::vec3(0.0f));
            ourShader.setVec3("pointLights[0].diffuse", glm::vec3(0.0f));
            ourShader.setVec3("pointLights[0].specular", glm::vec3(0.0f));
        }
        else {
            ourShader.setVec3("pointLights[0].ambient", flickerLightColor * 0.02f * flickerIntensity);
            ourShader.setVec3("pointLights[0].diffuse", flickerLightColor * flickerIntensity);
            ourShader.setVec3("pointLights[0].specular", flickerLightColor * flickerIntensity);
        }
        ourShader.setFloat("pointLights[0].constant", 1.0f);
        ourShader.setFloat("pointLights[0].linear", 0.09f);
        ourShader.setFloat("pointLights[0].quadratic", 0.032f);

        // --- PUNTO DE LUZ 1 (Luz del Teléfono / Aro de Luz del Monstruo) ---
        if (jumpscareActive && horrorLightOn)
        {
            glm::vec3 monsterFaceLightColor = glm::vec3(0.35f, 0.4f, 0.5f);

            ourShader.setVec3("pointLights[1].position", camera.Position);
            ourShader.setVec3("pointLights[1].ambient", monsterFaceLightColor * 0.15f);
            ourShader.setVec3("pointLights[1].diffuse", monsterFaceLightColor * 1.5f);
            ourShader.setVec3("pointLights[1].specular", monsterFaceLightColor * 0.8f);

            ourShader.setFloat("pointLights[1].constant", 1.0f);
            ourShader.setFloat("pointLights[1].linear", 0.35f);
            ourShader.setFloat("pointLights[1].quadratic", 0.45f);
        }
        else if (phoneRedLightOn)
        {
            float phoneFlicker = HorrorFlicker(currentFrame * 5.0f);
            glm::vec3 phoneLightColor = glm::vec3(0.9f, 0.0f, 0.0f);

            ourShader.setVec3("pointLights[1].position", phonePos);
            ourShader.setVec3("pointLights[1].ambient", phoneLightColor * 0.01f * phoneFlicker);
            ourShader.setVec3("pointLights[1].diffuse", phoneLightColor * phoneFlicker * 4.0f);
            ourShader.setVec3("pointLights[1].specular", phoneLightColor * phoneFlicker * 4.0f);

            ourShader.setFloat("pointLights[1].constant", 1.0f);
            ourShader.setFloat("pointLights[1].linear", 4.5f);
            ourShader.setFloat("pointLights[1].quadratic", 9.0f);
        }
        else
        {
            ourShader.setVec3("pointLights[1].diffuse", glm::vec3(0.0f));
            ourShader.setVec3("pointLights[1].specular", glm::vec3(0.0f));
            ourShader.setFloat("pointLights[1].constant", 1.0f);
            ourShader.setFloat("pointLights[1].linear", 0.09f);
            ourShader.setFloat("pointLights[1].quadratic", 0.032f);
        }

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

        // --- LÓGICA DE LA LINTERNA ---
        glm::vec3 handOffset = (camera.Right * linternaOffsetX) + (camera.Up * linternaOffsetY) + (camera.Front * linternaOffsetZ);
        glm::vec3 currentLinternaPos = camera.Position + handOffset;

        ourShader.setVec3("spotLight.position", camera.Position);
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
        // DIBUJADO DE MODELOS 3D
        // =========================================================================
        if (!jumpscareActive)
        {
            glm::mat4 garageMat = glm::mat4(1.0f);
            garageMat = glm::scale(garageMat, glm::vec3(garageScale));
            ourShader.setMat4("model", garageMat);
            garageModel.Draw(ourShader);

            if (crimeSceneRender) {
                glm::mat4 crimeSceneMat = glm::mat4(1.0f);
                crimeSceneMat = glm::translate(crimeSceneMat, crimeScenePos);
                crimeSceneMat = glm::scale(crimeSceneMat, glm::vec3(crimeSceneScale));
                ourShader.setMat4("model", crimeSceneMat);
                crimeSceneModel.Draw(ourShader);
            }

            glm::mat4 bodyMat = glm::mat4(1.0f);
            bodyMat = glm::translate(bodyMat, bodyOriginalPos);
            bodyMat = glm::rotate(bodyMat, glm::radians(bodyRotationY), glm::vec3(0.0f, 1.0f, 0.0f));
            bodyMat = glm::scale(bodyMat, glm::vec3(bodyScale));
            ourShader.setMat4("model", bodyMat);
            bodyModel.Draw(ourShader);

            glm::mat4 llantasMat = glm::mat4(1.0f);
            llantasMat = glm::translate(llantasMat, llantasPos);
            llantasMat = glm::scale(llantasMat, glm::vec3(llantasScale));
            ourShader.setMat4("model", llantasMat);
            llantasModel.Draw(ourShader);

            glm::mat4 phoneMat = glm::mat4(1.0f);
            phoneMat = glm::translate(phoneMat, phonePos);
            phoneMat = glm::scale(phoneMat, glm::vec3(phoneScale));
            ourShader.setMat4("model", phoneMat);
            phoneModel.Draw(ourShader);
        }
        else
        {
            glm::mat4 monsterMat = glm::mat4(1.0f);
            monsterMat = glm::translate(monsterMat, monsterPos);
            monsterMat = glm::rotate(monsterMat, glm::radians(-camera.Yaw - 90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            monsterMat = glm::rotate(monsterMat, glm::radians(camera.Pitch), glm::vec3(1.0f, 0.0f, 0.0f));
            monsterMat = glm::scale(monsterMat, glm::vec3(monsterScale * 1.3f));
            ourShader.setMat4("model", monsterMat);
            monsterModel.Draw(ourShader);
        }

        if (flashlightOn) {
            glm::mat4 linternaMat = glm::mat4(1.0f);
            linternaMat = glm::translate(linternaMat, currentLinternaPos);
            linternaMat = glm::rotate(linternaMat, glm::radians(-camera.Yaw - 90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            linternaMat = glm::rotate(linternaMat, glm::radians(camera.Pitch), glm::vec3(1.0f, 0.0f, 0.0f));
            linternaMat = glm::scale(linternaMat, glm::vec3(linternaScale));
            ourShader.setMat4("model", linternaMat);
            linternaModel.Draw(ourShader);
        }

        // =========================================================================
        // RENDERING HUD TEXT 
        // =========================================================================
        if (showMenu)
        {
            float startY = (float)SCR_HEIGHT - 80.0f;
            float stepY = 40.0f;
            RenderText(textShader, "W: al frente", 50.0f, startY, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f), (float)SCR_WIDTH, (float)SCR_HEIGHT);
            RenderText(textShader, "S: Atras", 50.0f, startY - stepY, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f), (float)SCR_WIDTH, (float)SCR_HEIGHT);
            RenderText(textShader, "A: Izquierda", 50.0f, startY - 2 * stepY, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f), (float)SCR_WIDTH, (float)SCR_HEIGHT);
            RenderText(textShader, "D: derecha", 50.0f, startY - 3 * stepY, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f), (float)SCR_WIDTH, (float)SCR_HEIGHT);
            RenderText(textShader, "F: prender/apagar linterna", 50.0f, startY - 4 * stepY, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f), (float)SCR_WIDTH, (float)SCR_HEIGHT);
            RenderText(textShader, "C: contestar", 50.0f, startY - 5 * stepY, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f), (float)SCR_WIDTH, (float)SCR_HEIGHT);
            RenderText(textShader, "M: ocultar/mostrar Menu", 50.0f, startY - 6 * stepY, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f), (float)SCR_WIDTH, (float)SCR_HEIGHT);

            std::string volText = "Volumen Musica: " + std::to_string(get_global_volume()) + "% (Flecha Arriba/Abajo)";
            RenderText(textShader, volText, 50.0f, startY - 7 * stepY, 0.5f, glm::vec3(0.0f, 1.0f, 0.5f), (float)SCR_WIDTH, (float)SCR_HEIGHT);
        }

        if (nearPhone && !callAnswered)
        {
            std::string alertText = "Presiona la tecla C para contestar la llamada";
            float scale = 0.6f;
            float textWidth = GetTextWidth(alertText, scale);
            float xPos = ((float)SCR_WIDTH - textWidth) / 2.0f;
            float yPos = 100.0f;

            RenderText(textShader, alertText, xPos + 2.0f, yPos - 2.0f, scale, glm::vec3(0.0f, 0.0f, 0.0f), (float)SCR_WIDTH, (float)SCR_HEIGHT);
            RenderText(textShader, alertText, xPos, yPos, scale, glm::vec3(0.9f, 0.8f, 0.2f), (float)SCR_WIDTH, (float)SCR_HEIGHT);
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
    float baseSpeed = debugMode ? 0.5f : 0.2f;
    camera.MovementSpeed = baseSpeed;

    // Control de volumen de la musica (flechas arriba y abajo)
    bool upPressed = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
    if (upPressed && !upKeyPressedLastFrame)
    {
        setvolume(get_global_volume() + 10);
    }
    upKeyPressedLastFrame = upPressed;

    bool downPressed = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
    if (downPressed && !downKeyPressedLastFrame)
    {
        setvolume(get_global_volume() - 10);
    }
    downKeyPressedLastFrame = downPressed;

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

    bool cPressed = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
    if (cPressed && !cKeyPressedLastFrame && nearPhone && !callAnswered)
    {
        haltmusic();
        playsound("music/Phone call.mp3", 0);
        callAnswered = true;
        phoneCallTimer = 0.0f;
        phoneRedLightOn = true;
    }
    cKeyPressedLastFrame = cPressed;

    bool mPressed = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
    if (mPressed && !mKeyPressedLastFrame)
    {
        showMenu = !showMenu;
    }
    mKeyPressedLastFrame = mPressed;

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
        if (!jumpscareActive) {
            ClampPlayerToGarage();
        }
    }

    bool fPressed = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    if (fPressed && !fKeyPressedLastFrame)
    {
        if (!jumpscareActive) {
            flashlightOn = !flashlightOn;
        }
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

    if (!jumpscareActive) {
        camera.ProcessMouseMovement(xoffset, yoffset);
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll((float)yoffset);
}