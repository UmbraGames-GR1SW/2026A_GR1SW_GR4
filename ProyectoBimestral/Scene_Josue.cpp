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
unsigned int loadTexture(char const* path); // Función para cargar la imagen 2D

bool pKeyPressedLastFrame = false;

// settings
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// =========================================================
// ESTADOS DE JUEGO (Mecánica de Jumpscare)
// =========================================================
enum GameState {
    PLAYING,
    JUMPSCARE,
    BLACK_SCREEN
};
GameState gameState = PLAYING;
float stateTimer = 0.0f; // Controla cuánto tiempo llevas en cada estado

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
float gameStartTime = 0.0f;

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
// LÍMITES DE COLISIÓN
// =========================================================
float roomMinX = -1.8f;
float roomMaxX = 2.1f;
float roomMinZ = -18.0f;
float roomMaxZ = 347.0f;
float playerHeight = 1.2f;

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

    // =========================================================
    // PREPARACIÓN DE LA IMAGEN 2D PARA EL JUMPSCARE
    // =========================================================
    // Cargamos la textura del grito
    unsigned int screamTexture = loadTexture("./model/exit/scream.jpeg");

    // Coordenadas de un rectángulo que cubre toda la pantalla (NDC)
    float quadVertices[] = {
        // Posiciones   // Texturas
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    // Compilamos un mini-shader directamente en C++ para no obligarte a crear más archivos
    const char* vShaderCode = "#version 330 core\nlayout (location = 0) in vec2 aPos;\nlayout (location = 1) in vec2 aTexCoords;\nout vec2 TexCoords;\nvoid main() { TexCoords = aTexCoords; gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); }\n";
    const char* fShaderCode = "#version 330 core\nout vec4 FragColor;\nin vec2 TexCoords;\nuniform sampler2D screenTexture;\nvoid main() { FragColor = texture(screenTexture, TexCoords); }\n";

    unsigned int vertex, fragment, quadShaderProgram;
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    quadShaderProgram = glCreateProgram();
    glAttachShader(quadShaderProgram, vertex);
    glAttachShader(quadShaderProgram, fragment);
    glLinkProgram(quadShaderProgram);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    // =========================================================

    gameStartTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        float absoluteTime = (float)glfwGetTime();
        deltaTime = absoluteTime - lastFrame;
        lastFrame = absoluteTime;

        processInput(window); // Procesa controles (bloqueados si te asustaron)

        // Limpiamos pantalla
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (gameState == PLAYING)
        {
            // === MODO JUEGO NORMAL ===
            float currentSceneTime = absoluteTime - gameStartTime;
            if (currentSceneTime < 7.0f) {
                entitySpeed = 2.0f;
            }
            else {
                entitySpeed = 25.0f;
            }

            // (El Depth Test vuelve a prenderse para 3D)
            glEnable(GL_DEPTH_TEST);

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

            // Movimiento Entidad
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
            // COLISIÓN (Si te toca -> Cambio al estado JUMPSCARE)
            // =========================================================
            float dx = camera.Position.x - entityPos.x;
            float dz = camera.Position.z - entityPos.z;
            float captureDistance = sqrt((dx * dx) + (dz * dz));
            float collisionRadius = 2.0f;

            if (captureDistance < collisionRadius)
            {
                gameState = JUMPSCARE;           // Disparamos la imagen
                stateTimer = (float)glfwGetTime(); // Marcamos a qué hora empezó el susto
            }
        }
        else if (gameState == JUMPSCARE)
        {
            // === MODO JUMPSCARE ===
            // Desactivar Depth Test para que la imagen 2D flote sobre todo
            glDisable(GL_DEPTH_TEST);

            glUseProgram(quadShaderProgram);
            glBindVertexArray(quadVAO);
            glBindTexture(GL_TEXTURE_2D, screamTexture);
            glDrawArrays(GL_TRIANGLES, 0, 6); // Dibuja la imagen full screen

            // Si pasaron 1.5 segundos, pasamos a pantalla negra
            if (absoluteTime - stateTimer >= 1.5f) {
                gameState = BLACK_SCREEN;
                stateTimer = (float)glfwGetTime();
            }
        }
        else if (gameState == BLACK_SCREEN)
        {
            // === MODO PANTALLA NEGRA ===
            // (La pantalla ya se limpió de negro al principio del ciclo, no dibujamos nada)

            // Si pasaron 4.0 segundos, reseteamos las posiciones y volvemos a jugar
            if (absoluteTime - stateTimer >= 4.0f) {
                camera.Position = glm::vec3(0.0f, 1.2f, 1.0f);
                entityPos = glm::vec3(0.0f, 0.0f, -25.0f);

                gameStartTime = (float)glfwGetTime(); // Reset tiempo de entidad lenta
                gameState = PLAYING;                  // Vuelta al juego
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

// Función auxiliar para cargar la textura del jumpscare
unsigned int loadTexture(char const* path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1) format = GL_RED;
        else if (nrComponents == 3) format = GL_RGB;
        else if (nrComponents == 4) format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }
    return textureID;
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
    // BLOQUEAR CONTROLES SI NO ESTÁS JUGANDO
    // =========================================================
    // Si estás viendo el jumpscare o la pantalla negra, ignora botones de movimiento
    if (gameState != PLAYING) {
        return;
    }

    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camera.MovementSpeed = 25.4f;
    else
        camera.MovementSpeed = 8.0f;

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

    // Solo mover la cámara (mirar) si estás vivo
    if (gameState == PLAYING) {
        camera.ProcessMouseMovement(xoffset, yoffset);
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (gameState == PLAYING) {
        camera.ProcessMouseScroll((float)yoffset);
    }
}