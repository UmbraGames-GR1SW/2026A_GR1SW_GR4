// ==============================
//    ESCENA VR ROOM - Josue (Final / Persecución)
// ==============================
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>
#include <learnopengl/audio.h>
#include <learnopengl/text_renderer.h>

#include <iostream>
#include <string>
#include <filesystem>

#include <learnopengl/stb_image.h>

extern enum SceneType { SCENE_PASILLO, SCENE_SAMY, SCENE_ANI, SCENE_MATTHEW, SCENE_JOSUE };
extern SceneType g_CurrentScene;
extern SceneType g_NextScene;
extern int g_UnlockedLevel;

namespace Josue {
    void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    void mouse_callback(GLFWwindow* window, double xpos, double ypos);
    void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    void processInput(GLFWwindow* window);
    void ClampPlayerToRoom();
    unsigned int loadTexture(char const* path);

    static bool pKeyPressedLastFrame = false;
    static bool upKeyPressedLastFrame = false;
    static bool downKeyPressedLastFrame = false;

    // settings
    static unsigned int SCR_WIDTH = 1280;
    static unsigned int SCR_HEIGHT = 720;

    // =========================================================
    // ESTADOS DE JUEGO (Mecánica de Jumpscare y Finales)
    // =========================================================
    enum GameState {
        PLAYING,
        JUMPSCARE,
        BLACK_SCREEN_RESET, // Pantalla negra tras perder (jumpscare)
        ENDING_WAIT_BLACK,  // 4 segundos de negro total al escapar
        ENDING_FADE_IN_1,   // Difuminado de negro a Texto 1
        ENDING_SHOW_1,      // Mostrar Texto 1 durante 12 segundos
        ENDING_FADE_OUT_1,  // Difuminado de Texto 1 a negro
        ENDING_FADE_IN_2,   // Difuminado de negro a Texto 2
        ENDING_WAIT_ENTER   // Pantalla final (Texto 2, espera ENTER)
    };
    static GameState gameState = PLAYING;
    static float stateTimer = 0.0f;
    static int selectedOption = 0; // 0 = Aceptar mi destino, 1 = No aceptar

    // =========================================================
    // ESCALA Y POSICIÓN
    // =========================================================
    static float worldScale = 0.05f;
    static Camera camera(glm::vec3(0.0f, 1.2f, 1.0f));
    static float lastX = 1280.0f / 2.0f;
    static float lastY = 720.0f / 2.0f;
    static bool firstMouse = true;

    // timing
    static float deltaTime = 0.0f;
    static float lastFrame = 0.0f;
    static float gameStartTime = 0.0f;

    // --- Entidad que persigue al jugador ---
    static glm::vec3 entityPos = glm::vec3(0.0f, 0.0f, -25.0f);
    static float entitySpeed = 2.0f;
    static bool entityInitialized = false;

    // =========================================================
    // VARIABLES DE LA LINTERNA
    // =========================================================
    static bool flashlightOn = true;
    static bool fKeyPressedLastFrame = false;
    static float linternaOffsetX = 0.015f;
    static float linternaOffsetY = -0.085f;
    static float linternaOffsetZ = 0.18f;

    // =========================================================
    // LÍMITES DE COLISIÓN
    // =========================================================
    static float roomMinX = -1.8f;
    static float roomMaxX = 2.1f;
    static float roomMinZ = -18.0f;
    static float roomMaxZ = 347.0f; // Límite donde se dispara el final
    static float playerHeight = 1.2f;

    // =========================================================
    // COORDENADAS BASE DE LOS LETREROS (Del Piso)
    // =========================================================
    static float baseLightX[5] = { 0.10074f,   0.259016f,  0.0407524f, 0.0422577f, 0.113721f };
    static float baseLightZ[5] = { -0.387055f, 17.4112f,   32.5504f,   50.2641f,   65.4025f };
    static float roofHeightY = 4.0f;

    // =========================================================
    // AUDIO - FLAGS DE CONTROL
    // =========================================================
    static bool ambientPlayed = false;
    static bool screamPlayed = false;

    void RunScene(GLFWwindow* window)
    {
        // Registrar callbacks locales para esta escena
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetScrollCallback(window, scroll_callback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // Obtener tamaño actual de la ventana
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        SCR_WIDTH = width;
        SCR_HEIGHT = height;

        // Reiniciar variables de estado locales
        gameState = PLAYING;
        stateTimer = 0.0f;
        selectedOption = 0;
        camera = Camera(glm::vec3(0.0f, 1.2f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), 90.0f, 0.0f);
        firstMouse = true;
        entityPos = glm::vec3(0.0f, 0.0f, -25.0f);
        entitySpeed = 2.0f;
        flashlightOn = true;
        fKeyPressedLastFrame = false;
        ambientPlayed = false;
        screamPlayed = false;

        // --- INICIALIZAR AUDIO ---
        initaudio();
        setvolume(100);

        // --- PRECARGAR el grito ANTES del render loop ---
        preloadSound("music/scream.mp3", "screamsound");

        Shader ourShader("shaders/Vertex_Josue.vs", "shaders/Fragment_Josue.fs");
        Model exitModel("./model/exit/exit.obj");
        Model entidadModel("./model/exit/entidad.obj");

        // Cargar fuentes para el menú interactivo final
        InitFreeType("fonts/arial.ttf");
        Shader textShader("shaders/text_samy.vs", "shaders/text_samy.fs");

        // Cargar texturas para las pantallas completas (Jumpscare y Final)
        unsigned int screamTexture = loadTexture("./model/exit/scream.jpeg");

        // Carga imágenes de texto 
        unsigned int ending1Texture = loadTexture("./model/exit/ending1.jpg");
        unsigned int ending2Texture = loadTexture("./model/exit/ending2.jpeg");

        float quadVertices[] = {
            // Posiciones   // Texturas
            -1.0f,  1.0f,  0.0f, 0.0f,
            -1.0f, -1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 1.0f,
            -1.0f,  1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 1.0f,
             1.0f,  1.0f,  1.0f, 0.0f
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

        const char* vShaderCode = "#version 330 core\nlayout (location = 0) in vec2 aPos;\nlayout (location = 1) in vec2 aTexCoords;\nout vec2 TexCoords;\nvoid main() { TexCoords = aTexCoords; gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); }\n";
        // Nuevo Fragment Shader que incluye la variable uniform 'fade' para difuminar la imagen a negro
        const char* fShaderCode = "#version 330 core\nout vec4 FragColor;\nin vec2 TexCoords;\nuniform sampler2D screenTexture;\nuniform float fade;\nvoid main() { FragColor = vec4(texture(screenTexture, TexCoords).rgb * fade, 1.0); }\n";

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
        glUseProgram(quadShaderProgram);
        glUniform1i(glGetUniformLocation(quadShaderProgram, "screenTexture"), 0);
        glDeleteShader(vertex);
        glDeleteShader(fragment);

        gameStartTime = (float)glfwGetTime();
        lastFrame = (float)glfwGetTime();

        while (!glfwWindowShouldClose(window) && g_NextScene == g_CurrentScene)
        {
            float absoluteTime = (float)glfwGetTime();
            deltaTime = absoluteTime - lastFrame;
            lastFrame = absoluteTime;

            processInput(window);

            // --- AUDIO: música de fondo apenas arranca el juego ---
            if (!ambientPlayed)
            {
                playmusic("music/ambiente.mp3");
                ambientPlayed = true;
            }

            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            if (gameState == PLAYING)
            {
                // =========================================================
                // LÓGICA DE ESCAPE ÉXITOSO (Trigger del Final)
                // =========================================================
                if (camera.Position.z >= 345.0f) // Llegó casi al final del pasillo
                {
                    gameState = ENDING_WAIT_BLACK; // Comienza con los 4 segundos en negro
                    stateTimer = absoluteTime;
                    entitySpeed = 0.0f; // Detener a la entidad
                    haltmusic();        // Silenciar la música
                    continue; // Saltar el renderizado del pasillo en este frame
                }

                // Dificultad dinámica
                float currentSceneTime = absoluteTime - gameStartTime;
                if (currentSceneTime < 7.0f) {
                    entitySpeed = 2.0f;
                }
                else {
                    entitySpeed = 25.0f;
                }

                glEnable(GL_DEPTH_TEST);
                ourShader.use();

                // Proyección y Vista
                glfwGetFramebufferSize(window, &width, &height);
                SCR_WIDTH = width;
                SCR_HEIGHT = height;
                glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 500.0f);
                glm::mat4 view = camera.GetViewMatrix();
                ourShader.setMat4("projection", projection);
                ourShader.setMat4("view", view);
                ourShader.setVec3("viewPos", camera.Position);

                // Luz Direccional (Ambiental oscura)
                ourShader.setVec3("lightDir", glm::vec3(-0.3f, -1.0f, -0.2f));
                ourShader.setVec3("lightColor", glm::vec3(0.02f, 0.02f, 0.03f));
                ourShader.setFloat("specularStrength", 0.05f);

                // Posición de Linterna (Mano de Elena)
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

                // Dibujar los Pasillos
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

                    // Luces Rojas Titilantes
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

                // Movimiento Entidad (Persecución)
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
                // COLISIÓN (Si te alcanza -> JUMPSCARE)
                // =========================================================
                float dx = camera.Position.x - entityPos.x;
                float dz = camera.Position.z - entityPos.z;
                float captureDistance = sqrt((dx * dx) + (dz * dz));
                float collisionRadius = 2.0f;

                if (captureDistance < collisionRadius)
                {
                    gameState = JUMPSCARE;
                    stateTimer = (float)glfwGetTime();

                    haltmusic();
                    if (!screamPlayed)
                    {
                        playPreloaded("screamsound");
                        screamPlayed = true;
                    }
                }
            }
            else if (gameState == JUMPSCARE)
            {
                glDisable(GL_DEPTH_TEST);
                glUseProgram(quadShaderProgram);
                glUniform1f(glGetUniformLocation(quadShaderProgram, "fade"), 1.0f); // Sin difuminado para el screamer
                glActiveTexture(GL_TEXTURE0);
                glBindVertexArray(quadVAO);
                glBindTexture(GL_TEXTURE_2D, screamTexture);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                if (absoluteTime - stateTimer >= 1.5f) {
                    gameState = BLACK_SCREEN_RESET;
                    stateTimer = (float)glfwGetTime();
                }
            }
            else if (gameState == BLACK_SCREEN_RESET)
            {
                // Espera en negro puro y reinicia la persecución
                if (absoluteTime - stateTimer >= 4.0f) {
                    camera.Position = glm::vec3(0.0f, 1.2f, 1.0f);
                    entityPos = glm::vec3(0.0f, 0.0f, -25.0f);

                    gameStartTime = (float)glfwGetTime();
                    gameState = PLAYING;

                    screamPlayed = false;
                    ambientPlayed = false;
                }
            }
            // =========================================================
            // LÓGICA DE FASES DEL FINAL (Pantallas con textos)
            // =========================================================
            else if (gameState >= ENDING_WAIT_BLACK)
            {
                glDisable(GL_DEPTH_TEST);
                glUseProgram(quadShaderProgram);
                glBindVertexArray(quadVAO);

                float fadeValue = 1.0f;
                unsigned int currentTexture = ending1Texture;
                float fadeDuration = 2.5f; // Duración del difuminado en segundos

                if (gameState == ENDING_WAIT_BLACK)
                {
                    fadeValue = 0.0f; // Completamente negro
                    if (absoluteTime - stateTimer >= 4.0f) { // Espera 4 segundos
                        gameState = ENDING_FADE_IN_1;
                        stateTimer = absoluteTime;
                    }
                }
                else if (gameState == ENDING_FADE_IN_1)
                {
                    float progress = (absoluteTime - stateTimer) / fadeDuration;
                    fadeValue = glm::clamp(progress, 0.0f, 1.0f);

                    if (progress >= 1.0f) {
                        gameState = ENDING_SHOW_1;
                        stateTimer = absoluteTime;
                    }
                }
                else if (gameState == ENDING_SHOW_1)
                {
                    fadeValue = 1.0f;
                    if (absoluteTime - stateTimer >= 12.0f) { // Mostrar la imagen 1 por 12 segundos
                        gameState = ENDING_FADE_OUT_1;
                        stateTimer = absoluteTime;
                    }
                }
                else if (gameState == ENDING_FADE_OUT_1)
                {
                    float progress = (absoluteTime - stateTimer) / fadeDuration;
                    fadeValue = 1.0f - glm::clamp(progress, 0.0f, 1.0f);

                    if (progress >= 1.0f) {
                        gameState = ENDING_FADE_IN_2;
                        stateTimer = absoluteTime;
                    }
                }
                else if (gameState == ENDING_FADE_IN_2)
                {
                    currentTexture = ending2Texture;
                    float progress = (absoluteTime - stateTimer) / fadeDuration;
                    fadeValue = glm::clamp(progress, 0.0f, 1.0f);

                    if (progress >= 1.0f) {
                        gameState = ENDING_WAIT_ENTER;
                        stateTimer = absoluteTime;
                    }
                }
                else if (gameState == ENDING_WAIT_ENTER)
                {
                    currentTexture = ending2Texture;
                    fadeValue = 1.0f; // Queda completamente visible esperando a que pulse ENTER
                }

                // Enviar la transparencia al shader y dibujar
                glUniform1f(glGetUniformLocation(quadShaderProgram, "fade"), fadeValue);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, currentTexture);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                if (gameState == ENDING_WAIT_ENTER) {
                    float sw = static_cast<float>(SCR_WIDTH);
                    float sh = static_cast<float>(SCR_HEIGHT);

                    // Título de Escoge tu destino
                    std::string promptText = "Escoge tu destino:";
                    float wp = GetTextWidth(promptText, 0.5f);
                    RenderText(textShader, promptText, (sw - wp) / 2.0f, sh / 2.0f - 30.0f, 0.5f, glm::vec3(0.9f, 0.9f, 0.9f), sw, sh);

                    // Opción 1
                    std::string opt1 = (selectedOption == 0) ? "> ACEPTAR MI DESTINO <" : "ACEPTAR MI DESTINO";
                    glm::vec3 col1 = (selectedOption == 0) ? glm::vec3(1.0f, 0.2f, 0.2f) : glm::vec3(0.7f, 0.7f, 0.7f);
                    float w1 = GetTextWidth(opt1, 0.6f);
                    RenderText(textShader, opt1, (sw - w1) / 2.0f, sh / 2.0f - 100.0f, 0.6f, col1, sw, sh);

                    // Opción 2
                    std::string opt2 = (selectedOption == 1) ? "> NO ACEPTAR <" : "NO ACEPTAR";
                    glm::vec3 col2 = (selectedOption == 1) ? glm::vec3(1.0f, 0.2f, 0.2f) : glm::vec3(0.7f, 0.7f, 0.7f);
                    float w2 = GetTextWidth(opt2, 0.6f);
                    RenderText(textShader, opt2, (sw - w2) / 2.0f, sh / 2.0f - 180.0f, 0.6f, col2, sw, sh);
                }
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        haltmusic();
        haltsound();
        closePreloaded("screamsound");
        glDeleteVertexArrays(1, &quadVAO);
        glDeleteBuffers(1, &quadVBO);
        glDeleteProgram(quadShaderProgram);
    }

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

            // Set explicit texture parameters for safety
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            stbi_image_free(data);
        }
        else
        {
            std::cout << "ERROR::TEXTURE:: Failed to load " << path << ". Reason: " << stbi_failure_reason() << std::endl;
            // Fallback: Si no encuentra la imagen genera un pixel negro puro de forma segura
            glBindTexture(GL_TEXTURE_2D, textureID);
            unsigned char blackPixel[3] = { 0, 0, 0 };
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, blackPixel);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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

        // Si estamos en la pantalla final, navegar opciones y confirmar
        if (gameState == ENDING_WAIT_ENTER) {
            // Teclas flecha arriba o W
            if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                if (!upKeyPressedLastFrame) {
                    selectedOption = 0;
                    upKeyPressedLastFrame = true;
                }
            }
            else {
                upKeyPressedLastFrame = false;
            }

            // Teclas flecha abajo o S
            if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                if (!downKeyPressedLastFrame) {
                    selectedOption = 1;
                    downKeyPressedLastFrame = true;
                }
            }
            else {
                downKeyPressedLastFrame = false;
            }

            if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
                if (selectedOption == 0) {
                    // ACEPTAR MI DESTINO -> Cerrar el juego
                    glfwSetWindowShouldClose(window, true);
                } else {
                    // NO ACEPTAR -> Regresar al Pasillo
                    g_NextScene = SCENE_PASILLO;
                }
            }
            return; // Bloquea todos los demás movimientos
        }

        // Si estamos en medio de las transiciones del final, bloquear el movimiento del jugador
        if (gameState >= ENDING_WAIT_BLACK) {
            return;
        }

        // Volver al Pasillo principal (Solo si está jugando normal o en reset)
        if (gameState == PLAYING || gameState == BLACK_SCREEN_RESET) {
            if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS)
                g_NextScene = SCENE_PASILLO;
        }

        if (gameState != PLAYING) {
            return;
        }

        // --- Bloquea el movimiento los primeros 5 segundos de cada partida ---
        float currentTime = (float)glfwGetTime();
        if (currentTime - gameStartTime < 5.0f) {
            return; // ESC y H ya se procesaron, el resto de movimiento se ignora
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
        SCR_WIDTH = width;
        SCR_HEIGHT = height;
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
}