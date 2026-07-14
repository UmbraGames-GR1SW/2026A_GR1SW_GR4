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

namespace Pasillo {
    static unsigned int SCR_WIDTH = 1920;
    static unsigned int SCR_HEIGHT = 1080;

    // -------------------------------------------------------------------------------------
    // ESTADOS PARA LA INTRODUCCIÓN CINEMÁTICA
    // -------------------------------------------------------------------------------------
    enum IntroPhase {
        INTRO_WAIT_BLACK, // Segundos en negro inicial (antes de mostrar imagen)
        INTRO_FADE_IN_IMG, // Aparece lentamente la imagen de bienvenida
        INTRO_WAIT_ENTER,  // Muestra la imagen esperando el ENTER
        INTRO_FADE_OUT_IMG,// Se desvanece la imagen a negro
        INTRO_FADE_IN_GAME,// El negro se desvanece revelando el pasillo
        INTRO_DONE         // El juego empieza normal
    };
    static IntroPhase introPhase = INTRO_WAIT_BLACK;
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

            stbi_image_free(data);
        }
        else
        {
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
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Lógica del ENTER para avanzar la intro
        bool enterPressed = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;
        if (enterPressed && !enterPressedLastFrame && introPhase == INTRO_WAIT_ENTER) {
            introPhase = INTRO_FADE_OUT_IMG;
            introTimer = (float)glfwGetTime();
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
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS)
            g_NextScene = SCENE_SAMY;
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS)
            g_NextScene = SCENE_ANI;
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS)
            g_NextScene = SCENE_MATTHEW;
        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)
            g_NextScene = SCENE_JOSUE;
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
        introPhase = INTRO_WAIT_BLACK;
        introTimer = static_cast<float>(glfwGetTime());
        enterPressedLastFrame = false;

        // Shaders
        Shader ourShader("shaders/Vertex_Samy.vs", "shaders/Fragment_Samy.fs");
        Shader textShader("shaders/text_samy.vs", "shaders/text_samy.fs");

        // Cargar modelo del pasillo
        Model pasilloModel("./model/pasillo/pasillo.obj");

        // Cargar fuentes
        InitFreeType("fonts/arial.ttf");

        // ---- CONFIGURACIÓN DEL QUAD Y SHADERS PARA LA PANTALLA DE INTRO ----
        unsigned int introTexture = loadTexture("./model/pasillo/intro_message.jpg"); // Asegúrate de tener esta imagen

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

            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

                if (introPhase == INTRO_WAIT_BLACK)
                {
                    if (currentFrame - introTimer >= 1.5f) {
                        introPhase = INTRO_FADE_IN_IMG;
                        introTimer = currentFrame;
                    }
                }
                else if (introPhase == INTRO_FADE_IN_IMG)
                {
                    float progress = (currentFrame - introTimer) / fadeDuration;
                    fadeValue = glm::clamp(progress, 0.0f, 1.0f);
                    if (progress >= 1.0f) {
                        introPhase = INTRO_WAIT_ENTER;
                    }
                }
                else if (introPhase == INTRO_WAIT_ENTER)
                {
                    fadeValue = 1.0f;
                    // Se queda esperando estático el ENTER (sin renderizar texto extra encima)
                }
                else if (introPhase == INTRO_FADE_OUT_IMG)
                {
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
                }

                if (!drawGameBehind) {
                    glUniform1f(glGetUniformLocation(quadShaderProgram, "fade"), fadeValue);
                    glBindTexture(GL_TEXTURE_2D, introTexture);
                    glDrawArrays(GL_TRIANGLES, 0, 6);

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
                RenderText(textShader, "1 o G -> Garage", 50.0f, startY - 9 * stepY, 0.5f, glm::vec3(0.9f, 0.2f, 0.2f), sw, sh);
                RenderText(textShader, "2 o I -> Infancia", 50.0f, startY - 10 * stepY, 0.5f, glm::vec3(0.9f, 0.2f, 0.2f), sw, sh);
                RenderText(textShader, "3 o T -> Navidad", 50.0f, startY - 11 * stepY, 0.5f, glm::vec3(0.9f, 0.2f, 0.2f), sw, sh);
                RenderText(textShader, "4 o J -> Final", 50.0f, startY - 12 * stepY, 0.5f, glm::vec3(0.9f, 0.2f, 0.2f), sw, sh);
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        glDeleteVertexArrays(1, &quadVAO);
        glDeleteBuffers(1, &quadVBO);
        glDeleteProgram(quadShaderProgram);
    }
}