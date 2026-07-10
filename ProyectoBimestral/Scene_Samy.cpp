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

bool backupFlashlightState = true;

// -------------------------------------------------------------------------------------
// EVENTOS Y TRASLADOS 
// -------------------------------------------------------------------------------------
bool crimeSceneRender = true;
bool horrorLightOn = false;

glm::vec3 crimeScenePos = glm::vec3(-0.05f, -0.228f, 0.60f);
glm::vec3 bodyOriginalPos = glm::vec3(-0.08f, -0.22f, 0.50f);
glm::vec3 llantasPos = glm::vec3(0.08f, -0.19f, -0.9f);
glm::vec3 monsterPos = glm::vec3(0.00f, -50.0f, -0.5f);
glm::vec3 phonePos = glm::vec3(-0.4f, 0.03f, -0.9f);


// AJUSTES DE ESCALA Y ROTACIÓN GENERAL
float bodyScale = 0.018f;
float crimeSceneScale = 0.082f;
float bodyRotationY = -45.0f;
float llantasScale = 0.1f;
float monsterScale = 0.10f;
float phoneScale = 0.0016f;

// =====================================================================================
// *** PANEL DE CONTROL DEL MONSTRUO Y EVENTOS DE INTERACCIÓN
// =====================================================================================
bool jumpscareActive = false;
bool jumpscareFinished = false;
float jumpscareTimer = 0.0f;

float monsterHeightOffset = -0.205f;
float monsterDistanceOffset = 0.22f;

// Variables de control de audio y secuencias
bool fondoPlayed = false;
bool phoneRingPlayed = false;
bool phoneCallPlayed = false;
bool nearPhone = false;
bool callAnswered = false;

float phoneCallTimer = 0.0f;

// Control del mensaje en pantalla (1.0 segundo)
bool messageTriggered = false;
float messageTimer = 0.0f;
bool showPaperMessage = false;

// Control de la luz roja de los teléfonos
bool phoneRedLightOn = false;
float phoneRedLightTimer = 0.0f;

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

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Garage - Horror Scene", NULL, NULL);
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

    Shader ourShader("shaders/Vertex_Samy.vs", "shaders/Fragment_Samy.fs");

    // ---- CARGA DE MODELOS ----
    Model garageModel("./model/garage/garage.obj");
    Model crimeSceneModel("./model/crime_scene/crime_scene.obj");
    Model bodyModel("./model/body/body.obj");
    Model llantasModel("./model/llantas/llantas.obj");
    Model monsterModel("./model/monster/monster.obj");
    Model linternaModel("./model/linterna/linterna.obj");
    Model phoneModel("./model/phone/phone.obj");

    // =========================================================================
    // CARGA MANUAL DE TEXTURA Y GEOMETRÍA PARA EL MENSAJE C (HUD)
    // =========================================================================
    unsigned int messageTexture;
    glGenTextures(1, &messageTexture);
    glBindTexture(GL_TEXTURE_2D, messageTexture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    // IMPORTANTE: Asegúrate de que el nombre del archivo y carpeta sean exactos
    unsigned char* data = stbi_load("texture/mensaje C.jpg", &width, &height, &nrChannels, 0);
    if (data)
    {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        std::cout << "ADVERTENCIA: No se pudo cargar la imagen 'texture/mensaje C.jpg'." << std::endl;
    }
    stbi_image_free(data);

    // Creamos el rectángulo (Quad) manualmente para la imagen
    float quadVertices[] = {
        // Posiciones        // Normales         // Coords textura
        -1.0f,  0.3f, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f,
        -1.0f, -0.3f, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f,
         1.0f, -0.3f, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f,

        -1.0f,  0.3f, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f,
         1.0f, -0.3f, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f,
         1.0f,  0.3f, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f
    };
    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    // =========================================================================

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
            phoneRedLightTimer = 0.0f;
        }

        // 3. Manejo de luz roja ambiental
        if (phoneRedLightOn)
        {
            phoneRedLightTimer += deltaTime;
            if (phoneRedLightTimer >= 10.0f && !jumpscareActive)
            {
                phoneRedLightOn = false;
            }
        }

        // 4. Lógica de Distancia y Temporizador del Mensaje (1.0 segundo)
        float distanceToPhone = glm::distance(camera.Position, phonePos);

        if (phoneRingPlayed && !callAnswered)
        {
            if (distanceToPhone < 0.35f)
            {
                nearPhone = true;

                if (!messageTriggered)
                {
                    messageTriggered = true;
                    messageTimer = 0.0f;
                }
            }
            else
            {
                nearPhone = false;
                messageTriggered = false; // Se reinicia si te alejas
            }
        }
        else
        {
            nearPhone = false;
        }

        // 5. Determinar si se dibuja el papel
        if (messageTriggered && !callAnswered)
        {
            messageTimer += deltaTime;
            // Se muestra exactamente 1 segundo
            if (messageTimer <= 1.0f)
            {
                showPaperMessage = true;
            }
            else
            {
                showPaperMessage = false;
            }
        }
        else
        {
            showPaperMessage = false;
        }

        // 6. Flujo post-interacción (Tecla C contestada)
        if (callAnswered && !jumpscareFinished)
        {
            phoneCallTimer += deltaTime;

            if (!jumpscareActive && phoneCallTimer >= 2.0f)
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

        // =========================================================================
        // RENDERIZADO Y CONTROL DE SHADERS
        // =========================================================================
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

        // --- PUNTO DE LUZ 1 (Luz del Monstruo O Luz Roja sobre el Teléfono) ---
        if (horrorLightOn && jumpscareActive)
        {
            float horrorFlicker = HorrorFlicker(currentFrame * 8.0f);
            glm::vec3 horrorLightColor = glm::vec3(0.9f, 0.0f, 0.0f);

            ourShader.setVec3("pointLights[1].position", monsterPos);
            ourShader.setVec3("pointLights[1].ambient", horrorLightColor * 0.05f * horrorFlicker);
            ourShader.setVec3("pointLights[1].diffuse", horrorLightColor * horrorFlicker * 2.5f);
            ourShader.setVec3("pointLights[1].specular", horrorLightColor * horrorFlicker * 2.5f);
        }
        else if (phoneRedLightOn)
        {
            float phoneFlicker = HorrorFlicker(currentFrame * 5.0f);
            glm::vec3 phoneLightColor = glm::vec3(0.8f, 0.0f, 0.0f);

            ourShader.setVec3("pointLights[1].position", phonePos);
            ourShader.setVec3("pointLights[1].ambient", phoneLightColor * 0.04f * phoneFlicker);
            ourShader.setVec3("pointLights[1].diffuse", phoneLightColor * phoneFlicker * 2.0f);
            ourShader.setVec3("pointLights[1].specular", phoneLightColor * phoneFlicker * 2.0f);
        }
        else
        {
            ourShader.setVec3("pointLights[1].diffuse", glm::vec3(0.0f));
            ourShader.setVec3("pointLights[1].specular", glm::vec3(0.0f));
        }
        ourShader.setFloat("pointLights[1].constant", 1.0f);
        ourShader.setFloat("pointLights[1].linear", 0.09f);
        ourShader.setFloat("pointLights[1].quadratic", 0.032f);

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
        // DIBUJADO DE MODELOS 3D
        // =========================================================================

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

        if (jumpscareActive)
        {
            glm::mat4 monsterMat = glm::mat4(1.0f);
            monsterMat = glm::translate(monsterMat, monsterPos);
            monsterMat = glm::rotate(monsterMat, glm::radians(-camera.Yaw - 90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            monsterMat = glm::rotate(monsterMat, glm::radians(camera.Pitch), glm::vec3(1.0f, 0.0f, 0.0f));
            monsterMat = glm::scale(monsterMat, glm::vec3(monsterScale * 1.3f));
            ourShader.setMat4("model", monsterMat);
            monsterModel.Draw(ourShader);
        }

        // =========================================================================
        // HUD DEL MENSAJE DE IMAGEN C (DURANTE 1.0 SEGUNDO)
        // =========================================================================
        if (showPaperMessage)
        {
            // Forzamos el shader a mostrar el mensaje ignorando la oscuridad (FullBright)
            bool previousDebugState = debugMode;
            ourShader.setBool("debugFullBright", true);

            glm::mat4 paperMat = glm::mat4(1.0f);

            // Lo ponemos justo frente a la cámara (a 25 cm) y un poco hacia abajo
            glm::vec3 hudOffset = (camera.Front * 0.25f) + (camera.Up * -0.05f);
            paperMat = glm::translate(paperMat, camera.Position + hudOffset);

            // Forzamos que la imagen SIEMPRE mire al frente (a tus ojos)
            paperMat = glm::rotate(paperMat, glm::radians(-camera.Yaw - 90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            paperMat = glm::rotate(paperMat, glm::radians(camera.Pitch), glm::vec3(1.0f, 0.0f, 0.0f));

            // Escala del rectángulo (puedes hacer este número más grande si la imagen se ve muy chiquita)
            paperMat = glm::scale(paperMat, glm::vec3(0.035f));
            ourShader.setMat4("model", paperMat);

            // Vinculamos la textura que cargamos de "mensaje C.jpg"
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, messageTexture);

            // Dibujamos nuestro Quad manual asegurándonos de que esté encima de todo (sin Z-buffer)
            glDisable(GL_DEPTH_TEST);
            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);

            // Restauramos la oscuridad del ambiente
            ourShader.setBool("debugFullBright", previousDebugState);
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

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Limpieza de memoria
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);

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

    bool cPressed = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
    if (cPressed && !cKeyPressedLastFrame && nearPhone && !callAnswered)
    {
        haltmusic();
        playsound("music/Phone call.mp3", 0);
        callAnswered = true;
        phoneCallTimer = 0.0f;
    }
    cKeyPressedLastFrame = cPressed;

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