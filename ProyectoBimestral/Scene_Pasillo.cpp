#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>
#include <learnopengl/text_renderer.h>
#include <learnopengl/stb_image.h>
#include <iostream>

extern enum SceneType { SCENE_PASILLO, SCENE_SAMY, SCENE_ANI, SCENE_MATTHEW, SCENE_JOSUE };
extern SceneType g_CurrentScene;
extern SceneType g_NextScene;
extern int g_UnlockedLevel;
extern bool g_ReturnedFromJosue;

namespace Pasillo {
    static unsigned int SCR_WIDTH = 1920;
    static unsigned int SCR_HEIGHT = 1080;

    // -------------------------------------------------------------------------------------
    // ESTADOS PARA EL FINAL EN EL PASILLO
    // -------------------------------------------------------------------------------------
    enum HallwayEndingState {
        END_NONE,
        END_WAIT_1S,        // Espera de 1 segundo en el pasillo antes de mostrar ending_choose
        END_FADE_IN_2,      // Aparece lentamente la imagen de ending_choose
        END_WAIT_CHOICE,    // Muestra ending_choose esperando Y/N
        END_ACCEPTED,       // Si elige Y (Acepta destino) -> Reproduce video y cierra
        END_REJECTED        // Si elige N (No acepta) -> Muestra gamebucle y espera ENTER
    };
    static HallwayEndingState hallwayEndingState = END_NONE;
    static float hallwayEndingTimer = 0.0f;

    // -------------------------------------------------------------------------------------
    // ESTADOS PARA LA INTRODUCCIÓN CINEMÁTICA
    // -------------------------------------------------------------------------------------
    enum IntroPhase {
        INTRO_WAIT_BLACK,      // Segundos en negro inicial (antes de mostrar imagen 1)
        INTRO_FADE_IN_IMG_1,   // Aparece lentamente la imagen 1 de bienvenida
        INTRO_WAIT_ENTER_1,    // Muestra la imagen 1 esperando el ENTER
        INTRO_FADE_OUT_IMG_1,  // Se desvanece la imagen 1 a negro
        INTRO_WAIT_BLACK_2,    // Breve espera en negro
        INTRO_FADE_IN_IMG_2,   // Aparece lentamente la imagen 2
        INTRO_WAIT_ENTER_2,    // Muestra la imagen 2 con texto renderizado encima
        INTRO_FADE_OUT_IMG_2,  // Se desvanece la imagen 2 a negro
        INTRO_FADE_IN_GAME,    // El negro se desvanece revelando el pasillo
        INTRO_DONE             // El juego empieza normal
    };
    static IntroPhase introPhase = INTRO_DONE; // Inicializado en DONE para saltar la intro si ya se vio
    static bool firstTimeEver = true;         // Variable persistente
    static float introTimer = 0.0f;
    static bool enterPressedLastFrame = false;

    // Posición inicial dentro del pasillo (escalado a metros)
    static Camera camera(glm::vec3(-12.0f, 1.2f, 0.0f));
    static float lastX = 1920.0f / 2.0f;
    static float lastY = 1080.0f / 2.0f;
    static bool firstMouse = true;

    static float deltaTime = 0.0f;
    static float lastFrame = 0.0f;

    static bool flashlightOn = true;
    static bool fKeyPressedLastFrame = false;
    static bool showMenu = true;
    static bool mKeyPressedLastFrame = false;

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

    void ClampPlayerToPasillo()
    {
        // Mantener al jugador dentro de los límites del pasillo escalado
        camera.Position.x = glm::clamp(camera.Position.x, -12.5f, 13.0f);
        camera.Position.z = glm::clamp(camera.Position.z, -2.2f, 2.2f);
        camera.Position.y = 1.2f; // Altura fija de ojos
    }

    void framebuffer_size_callback(GLFWwindow* window, int width, int height)
    {
        glViewport(0, 0, width, height);
        SCR_WIDTH = width;
        SCR_HEIGHT = height;
    }

    void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
    {
        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);

        if (firstMouse)
        {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos;

        lastX = xpos;
        lastY = ypos;

        if (introPhase == INTRO_DONE) {
            camera.ProcessMouseMovement(xoffset, yoffset);
        }
    }

    void processInput(GLFWwindow* window)
    {
        // --- BLOQUEO DE CONTROLES DURANTE EL FINAL EN EL PASILLO ---
        if (hallwayEndingState != END_NONE) {
            if (hallwayEndingState == END_WAIT_CHOICE) {
                if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS) {
                    hallwayEndingState = END_ACCEPTED;
                    hallwayEndingTimer = static_cast<float>(glfwGetTime());
                }
                else if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) {
                    hallwayEndingState = END_REJECTED;
                    hallwayEndingTimer = static_cast<float>(glfwGetTime());
                }
            }
            else if (hallwayEndingState == END_REJECTED) {
                if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
                    // Reiniciar al pasillo (elegir salas)
                    camera = Camera(glm::vec3(-12.0f, 1.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 0.0f, 0.0f);
                    firstMouse = true;
                    hallwayEndingState = END_NONE;
                    introPhase = INTRO_DONE;
                }
            }
            return;
        }

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Lógica del ENTER para avanzar la intro
        bool enterPressed = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;
        if (enterPressed && !enterPressedLastFrame) {
            if (introPhase == INTRO_WAIT_ENTER_1) {
                introPhase = INTRO_FADE_OUT_IMG_1;
                introTimer = (float)glfwGetTime();
            }
            else if (introPhase == INTRO_WAIT_ENTER_2) {
                introPhase = INTRO_FADE_OUT_IMG_2;
                introTimer = (float)glfwGetTime();
            }
        }
        enterPressedLastFrame = enterPressed;

        // --- BLOQUEO DE CONTROLES DURANTE LA INTRODUCCIÓN ---
        if (introPhase != INTRO_DONE) {
            return;
        }

        // Velocidad base y de carrera con Shift (estilo Samy)
        float baseSpeed = 2.5f;
        bool isRunning = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
        camera.MovementSpeed = isRunning ? (baseSpeed * 2.0f) : baseSpeed;

        // Movimiento con WASD
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.ProcessKeyboard(RIGHT, deltaTime);

        ClampPlayerToPasillo();

        // Control de linterna
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
        {
            if (!fKeyPressedLastFrame)
            {
                flashlightOn = !flashlightOn;
                fKeyPressedLastFrame = true;
            }
        }
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_RELEASE)
        {
            fKeyPressedLastFrame = false;
        }

        // Mostrar / Ocultar menú con tecla M
        bool mPressed = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
        if (mPressed && !mKeyPressedLastFrame)
        {
            showMenu = !showMenu;
        }
        mKeyPressedLastFrame = mPressed;

        // Teclas para viajar a otras escenas (recuerdos) sin conflicto con WASD
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) {
            if (g_UnlockedLevel >= 1)
                g_NextScene = SCENE_SAMY;
        }
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) {
            if (g_UnlockedLevel >= 2)
                g_NextScene = SCENE_ANI;
        }
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
            if (g_UnlockedLevel >= 3)
                g_NextScene = SCENE_MATTHEW;
        }
        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) {
            if (g_UnlockedLevel >= 4)
                g_NextScene = SCENE_JOSUE;
        }
    }

    void RunScene(GLFWwindow* window)
    {
        // Configurar callbacks del window
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // Obtener tamaño actual de la ventana
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        SCR_WIDTH = width;
        SCR_HEIGHT = height;

        // Reiniciar variables para cuando se regrese a esta escena
        enterPressedLastFrame = false;

        // Comprobar si se regresó de la escena de Josué para disparar el final
        if (g_ReturnedFromJosue) {
            hallwayEndingState = END_WAIT_1S;
            hallwayEndingTimer = static_cast<float>(glfwGetTime());
            g_ReturnedFromJosue = false;
        } else {
            hallwayEndingState = END_NONE;
        }

        // Lógica de primera vez: solo inicializa la intro si es el primer arranque del programa
        if (firstTimeEver) {
            introPhase = INTRO_WAIT_BLACK;
            introTimer = static_cast<float>(glfwGetTime());
            firstTimeEver = false;
            // Cargar fuentes para la intro
            InitFreeType("fonts/Bevan-Regular.ttf");
        }
        else {
            introPhase = INTRO_DONE;
            // Cargar fuentes normales del juego
            InitFreeType("fonts/arial.ttf");
        }

        // Shaders
        Shader ourShader("shaders/Vertex_Samy.vs", "shaders/Fragment_Samy.fs");
        Shader textShader("shaders/text_samy.vs", "shaders/text_samy.fs");

        // Cargar modelo del pasillo
        Model pasilloModel("./model/pasillo/pasillo.obj");

        // ---- CONFIGURACIÓN DEL QUAD Y SHADERS PARA LA PANTALLA DE INTRO ----
        unsigned int introTexture = loadTexture("./model/pasillo/intro_message.jpg"); // Asegúrate de tener esta imagen
        unsigned int introTexture2 = loadTexture("./model/pasillo/intro_message_2.png");
        unsigned int endingChooseTexture = loadTexture("./model/exit/ending_choose.png");
        unsigned int gamebucleTexture = loadTexture("./model/exit/gamebucle.png");

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

        // Reiniciar estado de cámara apuntando por el pasillo (Yaw = 0.0f, X en el inicio a -12.0f)
        camera = Camera(glm::vec3(-12.0f, 1.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 0.0f, 0.0f);
        firstMouse = true;
        lastFrame = static_cast<float>(glfwGetTime());

        glEnable(GL_DEPTH_TEST);

        while (!glfwWindowShouldClose(window) && g_NextScene == g_CurrentScene)
        {
            float currentFrame = static_cast<float>(glfwGetTime());
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            processInput(window);

            if (hallwayEndingState == END_WAIT_1S) {
                if (currentFrame - hallwayEndingTimer >= 1.0f) {
                    hallwayEndingState = END_FADE_IN_2;
                    hallwayEndingTimer = currentFrame;
                }
            }

            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // -------------------------------------------------------------------------------------
            // LÓGICA DE FASE DEL FINAL EN EL PASILLO (ending_choose y gamebucle)
            // -------------------------------------------------------------------------------------
            if (hallwayEndingState != END_NONE && hallwayEndingState != END_WAIT_1S)
            {
                glDisable(GL_DEPTH_TEST);
                glUseProgram(quadShaderProgram);
                glBindVertexArray(quadVAO);

                float fadeValue = 0.0f;
                float fadeDuration = 2.0f;
                unsigned int activeTex = endingChooseTexture;

                if (hallwayEndingState == END_FADE_IN_2)
                {
                    float progress = (currentFrame - hallwayEndingTimer) / fadeDuration;
                    fadeValue = glm::clamp(progress, 0.0f, 1.0f);
                    if (progress >= 1.0f) {
                        hallwayEndingState = END_WAIT_CHOICE;
                    }
                }
                else if (hallwayEndingState == END_WAIT_CHOICE)
                {
                    fadeValue = 1.0f;
                    activeTex = endingChooseTexture;
                }
                else if (hallwayEndingState == END_ACCEPTED)
                {
                    // Acepta su destino -> Reproduce video de gameover y termina
                    system("start \"\" \"model/exit/gameover.mp4\"");
                    glfwSetWindowShouldClose(window, true);
                    fadeValue = 0.0f;
                }
                else if (hallwayEndingState == END_REJECTED)
                {
                    fadeValue = 1.0f;
                    activeTex = gamebucleTexture; // Muestra la imagen de gamebucle
                }

                glUniform1f(glGetUniformLocation(quadShaderProgram, "fade"), fadeValue);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, activeTex);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                // Renderizar textos explicativos sobre el quad del final si es necesario
                float sw = static_cast<float>(SCR_WIDTH);
                float sh = static_cast<float>(SCR_HEIGHT);

                if (hallwayEndingState == END_WAIT_CHOICE)
                {
                    // Opcional: mostrar texto explicativo de teclas (para interactuar)
                    std::string promptText = "Presiona [Y] para ACEPTAR TU DESTINO  /  [N] para NO ACEPTAR";
                    float wp = GetTextWidth(promptText, 0.4f);
                    RenderText(textShader, promptText, (sw - wp) / 2.0f, 50.0f, 0.4f, glm::vec3(0.8f, 0.8f, 0.8f), sw, sh);
                }
                else if (hallwayEndingState == END_REJECTED)
                {
                    // Explicar cómo reiniciar el bucle
                    std::string promptText = "Presiona ENTER para reiniciar el bucle de recuerdos";
                    float wp = GetTextWidth(promptText, 0.4f);
                    RenderText(textShader, promptText, (sw - wp) / 2.0f, 50.0f, 0.4f, glm::vec3(0.8f, 0.8f, 0.8f), sw, sh);
                }

                glfwSwapBuffers(window);
                glfwPollEvents();
                continue;
            }

            // -------------------------------------------------------------------------------------
            // LÓGICA DE LA FASE INTRODUCTORIA
            // -------------------------------------------------------------------------------------
            if (introPhase != INTRO_DONE)
            {
                glDisable(GL_DEPTH_TEST);
                glUseProgram(quadShaderProgram);
                glBindVertexArray(quadVAO);

                float fadeValue = 0.0f;
                float fadeDuration = 2.0f; // Duración de los difuminados
                bool drawGameBehind = false;
                unsigned int currentIntroTex = introTexture;

                if (introPhase == INTRO_WAIT_BLACK)
                {
                    if (currentFrame - introTimer >= 1.5f) {
                        introPhase = INTRO_FADE_IN_IMG_1;
                        introTimer = currentFrame;
                    }
                }
                else if (introPhase == INTRO_FADE_IN_IMG_1)
                {
                    currentIntroTex = introTexture;
                    float progress = (currentFrame - introTimer) / fadeDuration;
                    fadeValue = glm::clamp(progress, 0.0f, 1.0f);
                    if (progress >= 1.0f) {
                        introPhase = INTRO_WAIT_ENTER_1;
                    }
                }
                else if (introPhase == INTRO_WAIT_ENTER_1)
                {
                    currentIntroTex = introTexture;
                    fadeValue = 1.0f;
                }
                else if (introPhase == INTRO_FADE_OUT_IMG_1)
                {
                    currentIntroTex = introTexture;
                    float progress = (currentFrame - introTimer) / fadeDuration;
                    fadeValue = 1.0f - glm::clamp(progress, 0.0f, 1.0f);
                    if (progress >= 1.0f) {
                        introPhase = INTRO_WAIT_BLACK_2;
                        introTimer = currentFrame;
                    }
                }
                else if (introPhase == INTRO_WAIT_BLACK_2)
                {
                    fadeValue = 0.0f;
                    if (currentFrame - introTimer >= 1.0f) { // 1 segundo de negro intermedio
                        introPhase = INTRO_FADE_IN_IMG_2;
                        introTimer = currentFrame;
                    }
                }
                else if (introPhase == INTRO_FADE_IN_IMG_2)
                {
                    currentIntroTex = introTexture2;
                    float progress = (currentFrame - introTimer) / fadeDuration;
                    fadeValue = glm::clamp(progress, 0.0f, 1.0f);
                    if (progress >= 1.0f) {
                        introPhase = INTRO_WAIT_ENTER_2;
                    }
                }
                else if (introPhase == INTRO_WAIT_ENTER_2)
                {
                    currentIntroTex = introTexture2;
                    fadeValue = 1.0f;
                }
                else if (introPhase == INTRO_FADE_OUT_IMG_2)
                {
                    currentIntroTex = introTexture2;
                    float progress = (currentFrame - introTimer) / fadeDuration;
                    fadeValue = 1.0f - glm::clamp(progress, 0.0f, 1.0f);
                    if (progress >= 1.0f) {
                        introPhase = INTRO_FADE_IN_GAME;
                        introTimer = currentFrame;
                    }
                }
                else if (introPhase == INTRO_FADE_IN_GAME)
                {
                    drawGameBehind = true;
                    introPhase = INTRO_DONE;
                    InitFreeType("fonts/arial.ttf"); // Restaurar Arial para el HUD del juego
                }

                if (!drawGameBehind) {
                    glUniform1f(glGetUniformLocation(quadShaderProgram, "fade"), fadeValue);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, currentIntroTex);
                    glDrawArrays(GL_TRIANGLES, 0, 6);

                    // Renderizar textos sobre el quad de introducción
                    float sw = static_cast<float>(SCR_WIDTH);
                    float sh = static_cast<float>(SCR_HEIGHT);

                    // No programmatic text here; it's already in intro_message_2.png
                    /*
                    if (introPhase == INTRO_FADE_IN_IMG_2 || introPhase == INTRO_WAIT_ENTER_2 || introPhase == INTRO_FADE_OUT_IMG_2) {
                        float scale = 0.7f;
                        float lineSpacing = 65.0f;
                        float startY = sh / 2.0f + 120.0f;

                        const char* introLines[] = {
                            "Elena despierta en el pabellon 9 sin recordar porque esta en ese lugar.",
                            "Cada puerta esconde un fragmento de su mente que ella misma",
                            "enterro para sobrevivir.",
                            "Para transcender, primero debe recordar."
                        };
                        int numLines = 4;

                        for (int i = 0; i < numLines; ++i) {
                            float w = GetTextWidth(introLines[i], scale);
                            float x = (sw - w) / 2.0f;
                            float y = startY - i * lineSpacing;
                            RenderText(textShader, introLines[i], x, y, scale, glm::vec3(0.0f, 0.0f, 0.0f), sw, sh, fadeValue);
                        }
                    }
                    */

                    // Mensajes para continuar con ENTER
                    /*
                    if (introPhase == INTRO_WAIT_ENTER_1) {
                        std::string continueText = "[ Presione ENTER para ingresar ]";
                        float cw = GetTextWidth(continueText, 0.4f);
                        RenderText(textShader, continueText, (sw - cw) / 2.0f, 80.0f, 0.4f, glm::vec3(0.7f, 0.7f, 0.7f), sw, sh);
                    }
                    */
                    /*
                    else if (introPhase == INTRO_WAIT_ENTER_2) {
                        std::string continueText = "[ Presione ENTER para ingresar ]";
                        float cw = GetTextWidth(continueText, 0.5f);
                        RenderText(textShader, continueText, (sw - cw) / 2.0f, 80.0f, 0.5f, glm::vec3(0.0f, 0.0f, 0.0f), sw, sh);
                    }
                    */

                    glfwSwapBuffers(window);
                    glfwPollEvents();
                    continue; // Saltar el renderizado del nivel 3D
                }
            }

            // -------------------------------------------------------------------------------------
            // RENDERIZADO DEL PASILLO 3D
            // -------------------------------------------------------------------------------------
            glEnable(GL_DEPTH_TEST);
            ourShader.use();

            // Parámetros de la cámara
            ourShader.setVec3("viewPos", camera.Position);
            ourShader.setBool("debugFullBright", false);

            // Dirección de luz tenue
            ourShader.setVec3("dirLight.direction", glm::vec3(-0.2f, -1.0f, -0.3f));
            ourShader.setVec3("dirLight.ambient", glm::vec3(0.05f, 0.05f, 0.05f));
            ourShader.setVec3("dirLight.diffuse", glm::vec3(0.1f, 0.1f, 0.12f));
            ourShader.setVec3("dirLight.specular", glm::vec3(0.1f, 0.1f, 0.12f));

            // Luz SpotLight (linterna del jugador)
            ourShader.setVec3("spotLight.position", camera.Position);
            ourShader.setVec3("spotLight.direction", camera.Front);
            ourShader.setFloat("spotLight.constant", 1.0f);
            ourShader.setFloat("spotLight.linear", 0.09f);
            ourShader.setFloat("spotLight.quadratic", 0.032f);
            ourShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
            ourShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(17.5f)));

            if (flashlightOn)
            {
                ourShader.setVec3("spotLight.ambient", glm::vec3(0.0f));
                ourShader.setVec3("spotLight.diffuse", glm::vec3(2.0f, 2.0f, 1.8f));
                ourShader.setVec3("spotLight.specular", glm::vec3(1.2f, 1.2f, 1.0f));
            }
            else
            {
                ourShader.setVec3("spotLight.ambient", glm::vec3(0.0f));
                ourShader.setVec3("spotLight.diffuse", glm::vec3(0.0f));
                ourShader.setVec3("spotLight.specular", glm::vec3(0.0f));
            }

            // Inicializar a cero phoneLight y pointLights no usados en pasillo para evitar fallas en el shader
            ourShader.setVec3("phoneLight.position", glm::vec3(0.0f));
            ourShader.setVec3("phoneLight.direction", glm::vec3(0.0f, -1.0f, 0.0f));
            ourShader.setVec3("phoneLight.ambient", glm::vec3(0.0f));
            ourShader.setVec3("phoneLight.diffuse", glm::vec3(0.0f));
            ourShader.setVec3("phoneLight.specular", glm::vec3(0.0f));
            ourShader.setFloat("phoneLight.constant", 1.0f);
            ourShader.setFloat("phoneLight.linear", 0.09f);
            ourShader.setFloat("phoneLight.quadratic", 0.032f);
            ourShader.setFloat("phoneLight.cutOff", glm::cos(glm::radians(2.0f)));
            ourShader.setFloat("phoneLight.outerCutOff", glm::cos(glm::radians(3.5f)));

            for (int i = 0; i < 4; i++) {
                std::string base = "pointLights[" + std::to_string(i) + "].";
                ourShader.setVec3(base + "position", glm::vec3(0.0f));
                ourShader.setVec3("pointLights[" + std::to_string(i) + "].ambient", glm::vec3(0.0f));
                ourShader.setVec3("pointLights[" + std::to_string(i) + "].diffuse", glm::vec3(0.0f));
                ourShader.setVec3("pointLights[" + std::to_string(i) + "].specular", glm::vec3(0.0f));
                ourShader.setFloat("pointLights[" + std::to_string(i) + "].constant", 1.0f);
                ourShader.setFloat("pointLights[" + std::to_string(i) + "].linear", 0.09f);
                ourShader.setFloat("pointLights[" + std::to_string(i) + "].quadratic", 0.032f);
            }

            // Proyecciones
            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
            glm::mat4 view = camera.GetViewMatrix();
            ourShader.setMat4("projection", projection);
            ourShader.setMat4("view", view);

            // Dibujar el modelo del pasillo (escalado a metros ya que el original está en pulgadas)
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
            model = glm::scale(model, glm::vec3(0.0254f));
            ourShader.setMat4("model", model);
            pasilloModel.Draw(ourShader);

            // Dibujar HUD (Menú vertical al estilo Samy)
            float sw = static_cast<float>(SCR_WIDTH);
            float sh = static_cast<float>(SCR_HEIGHT);
            RenderText(textShader, "EL PABELLON 9 - RECUERDO SEPULTADO", 50.0f, sh - 60.0f, 0.6f, glm::vec3(0.8f, 0.8f, 0.9f), sw, sh);

            if (showMenu)
            {
                float startY = sh - 120.0f;
                float stepY = 40.0f;
                RenderText(textShader, "W: Al frente", 50.0f, startY, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f), sw, sh);
                RenderText(textShader, "S: Atras", 50.0f, startY - stepY, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f), sw, sh);
                RenderText(textShader, "A: Izquierda", 50.0f, startY - 2 * stepY, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f), sw, sh);
                RenderText(textShader, "D: Derecha", 50.0f, startY - 3 * stepY, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f), sw, sh);
                RenderText(textShader, "SHIFT + W: Correr", 50.0f, startY - 4 * stepY, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f), sw, sh);
                RenderText(textShader, "F: Prender/apagar linterna", 50.0f, startY - 5 * stepY, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f), sw, sh);
                RenderText(textShader, "M: Ocultar/mostrar Menu", 50.0f, startY - 6 * stepY, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f), sw, sh);

                RenderText(textShader, "Seleccione una puerta (recuerdo):", 50.0f, startY - 8 * stepY, 0.5f, glm::vec3(0.9f, 0.8f, 0.8f), sw, sh);
                RenderText(textShader, "1 o G -> Garage", 50.0f, startY - 9 * stepY, 0.5f, glm::vec3(0.2f, 0.9f, 0.2f), sw, sh);

                if (g_UnlockedLevel >= 2)
                    RenderText(textShader, "2 o I -> Infancia", 50.0f, startY - 10 * stepY, 0.5f, glm::vec3(0.2f, 0.9f, 0.2f), sw, sh);
                else
                    RenderText(textShader, "2 o I -> [BLOQUEADO] Infancia", 50.0f, startY - 10 * stepY, 0.5f, glm::vec3(0.5f, 0.5f, 0.5f), sw, sh);

                if (g_UnlockedLevel >= 3)
                    RenderText(textShader, "3 o T -> Navidad", 50.0f, startY - 11 * stepY, 0.5f, glm::vec3(0.2f, 0.9f, 0.2f), sw, sh);
                else
                    RenderText(textShader, "3 o T -> [BLOQUEADO] Navidad", 50.0f, startY - 11 * stepY, 0.5f, glm::vec3(0.5f, 0.5f, 0.5f), sw, sh);

                if (g_UnlockedLevel >= 4)
                    RenderText(textShader, "4 o J -> Final", 50.0f, startY - 12 * stepY, 0.5f, glm::vec3(0.2f, 0.9f, 0.2f), sw, sh);
                else
                    RenderText(textShader, "4 o J -> [BLOQUEADO] Final", 50.0f, startY - 12 * stepY, 0.5f, glm::vec3(0.5f, 0.5f, 0.5f), sw, sh);
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        glDeleteVertexArrays(1, &quadVAO);
        glDeleteBuffers(1, &quadVBO);
        glDeleteProgram(quadShaderProgram);
    }
}