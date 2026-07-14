// ==============================
//    ESCENA Anahi (Cuarto de juguetes)
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

#include <iostream>
#include <cmath>
#include <random>
#include <string>
#include <algorithm>

#include "text_renderer.h"
#include "learnopengl/stb_image.h"

extern enum SceneType { SCENE_PASILLO, SCENE_SAMY, SCENE_ANI, SCENE_MATTHEW, SCENE_JOSUE };
extern SceneType g_CurrentScene;
extern SceneType g_NextScene;

namespace Anahi {
    void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    void mouse_callback(GLFWwindow* window, double xpos, double ypos);
    void processInput(GLFWwindow* window);
    float HorrorFlicker(float time);
    float RevealFactor(float tiempoActual, float appearTime, float fadeDuration);
    void ClampPlayerToScene(float revealPayaso, float revealChuky, float revealHacha, float revealDanger, float revealMascara);

    // settings
    static unsigned int SCR_WIDTH = 800;
    static unsigned int SCR_HEIGHT = 600;

    // -------------------------------------------------------------------------------------
    // PERSONAJE / CAMARA
    // -------------------------------------------------------------------------------------
    static Camera camera(glm::vec3(0.0f, 1.2f, 4.0f));
    static float lastX = 800.0f / 2.0f;
    static float lastY = 600.0f / 2.0f;
    static bool firstMouse = true;

    static float bobbingTimer = 0.0f;
    static bool isMoving = false;

    // timing
    static float deltaTime = 0.0f;
    static float lastFrame = 0.0f;

    static bool flashlightOn = true;
    static bool fKeyPressedLastFrame = false;

    // pantalla de bienvenida
    static bool juegoIniciado = false;
    static bool enterPressedLastFrame = false;

    static glm::vec3 colorLuzTecho = glm::vec3(0.75f, 0.75f, 0.7f);

    // -------------------------------------------------------------------------------------
    // HUD
    // -------------------------------------------------------------------------------------
    static TextRenderer* textRenderer = nullptr;
    static bool showMenu = true;
    static bool mKeyPressedLastFrame = false;
    static bool upKeyPressedLastFrame = false;
    static bool downKeyPressedLastFrame = false;

    // -------------------------------------------------------------------------------------
    // AUDIO (audios/miedo.mp3 en bucle)
    // -------------------------------------------------------------------------------------
    static bool audioMiedoIniciado = false;

    // -------------------------------------------------------------------------------------
    // ENTIDADES Y PROPS
    // -------------------------------------------------------------------------------------
    static glm::vec3 clownPosition = glm::vec3(0.0f, 0.0f, -2.0f);
    static float clownScale = 0.0038f;
    static const float CLOWN_FEET_OFFSET = 8.926056f;

    static glm::vec3 chukyPosition = glm::vec3(-6.0f, 0.0f, 1.8f);
    static float chukyScale = 1.0f;
    static const float CHUKY_FEET_OFFSET = 0.00636f;

    static glm::vec3 hachaPosition = glm::vec3(4.5f, 0.0f, 0.0f);
    static float hachaScale = 4.0f;
    static float hachaRotationY = glm::radians(25.0f);

    static glm::vec3 dangerPosition = glm::vec3(-6.0f, 1.0f, -3.5f);
    static float dangerScale = 1.0f;
    static float dangerRotationY = glm::radians(60.0f);

    static glm::vec3 mascaraPosition = glm::vec3(-9.0f, 0.0f, 2.0f);
    static float mascaraScale = 1.0f;
    static float mascaraRotationY = glm::radians(90.0f);

    // -------------------------------------------------------------------------------------
    // COLISIONES (caja simple en XZ, sin depender de la clase Model)
    // Calibra con la tecla P: camina hasta cada pared y ajusta estos 4 valores.
    // -------------------------------------------------------------------------------------
    static float sceneMinX = -9.5f;
    static float sceneMaxX = 5.5f;
    static float sceneMinZ = -4.5f;
    static float sceneMaxZ = 3.0f;
    static bool pKeyPressedLastFrame = false;

    static const float CLOWN_COLLISION_RADIUS = 1.1f;   
    static const float CHUKY_COLLISION_RADIUS = 1.0f;   
    static const float HACHA_COLLISION_RADIUS = 0.8f;
    static const float DANGER_COLLISION_RADIUS = 0.6f;
    static const float MASCARA_COLLISION_RADIUS = 0.4f;

    // -------------------------------------------------------------------------------------
    // TIEMPOS DE APARICIÓN GRADUAL
    // -------------------------------------------------------------------------------------
    static const float CLOWN_APPEAR_TIME = 7.0f;
    static const float CHUKY_APPEAR_TIME = 12.0f;
    static const float HACHA_APPEAR_TIME = 6.0f;
    static const float DANGER_APPEAR_TIME = 7.0f;
    static const float MASCARA_APPEAR_TIME = 12.0f;
    static const float REVEAL_FADE_DURATION = 3.58f;

    static float tiempoInicioJuego = 0.0f;
    static bool tiempoInicioRegistrado = false;

    void RunScene(GLFWwindow* window)
    {
        // Registrar callbacks locales para esta escena
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetScrollCallback(window, NULL); // No hay scroll callback en esta escena
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // Reiniciar variables de estado
        camera = Camera(glm::vec3(0.0f, 1.2f, 4.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, 0.0f);
        firstMouse = true;
        juegoIniciado = false;
        enterPressedLastFrame = false;
        audioMiedoIniciado = false;
        tiempoInicioRegistrado = false;
        tiempoInicioJuego = 0.0f;

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        SCR_WIDTH = width;
        SCR_HEIGHT = height;

        initaudio();

        textRenderer = new TextRenderer(SCR_WIDTH, SCR_HEIGHT);
        if (!textRenderer->Load("fonts/Bevan-Regular.ttf", 48))
            std::cout << "ADVERTENCIA: No se pudo cargar la fuente del HUD" << std::endl;

        Shader ourShader("shaders/Vertex_Anahi.vs", "shaders/Fragment_Anahi.fs");

        // ---- CARGA DE MODELOS ----
        Model garageModel("./model/garage/garage.obj");
        Model clownModel("./model/payaso/payaso.obj");
        Model chukyModel("./model/chuky/chuky.obj");
        Model hachaModel("./model/hacha/hacha.obj");
        Model dangerModel("./model/danger/danger.obj");
        Model mascaraModel("./model/mascara/mascara.obj");

        camera.MovementSpeed = 2.5f;

        std::default_random_engine generator;
        std::uniform_real_distribution<float> distribution(-0.008f, 0.008f);

        lastFrame = (float)glfwGetTime();

        // render loop
        while (!glfwWindowShouldClose(window) && g_NextScene == g_CurrentScene)
        {
            float currentFrame = (float)glfwGetTime();
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            processInput(window);

            if (juegoIniciado && !tiempoInicioRegistrado)
            {
                tiempoInicioJuego = currentFrame;
                tiempoInicioRegistrado = true;
            }
            float tiempoDeJuego = juegoIniciado ? (currentFrame - tiempoInicioJuego) : 0.0f;

            if (juegoIniciado && !audioMiedoIniciado)
            {
                playsound("audios/miedo.mp3", 1);
                audioMiedoIniciado = true;
            }

            // head bobbing
            if (isMoving)
            {
                bobbingTimer += deltaTime * 12.0f;
                camera.Position.y = 1.2f + sin(bobbingTimer) * 0.05f;
            }
            else
            {
                bobbingTimer = 0.0f;
                camera.Position.y = 1.2f;
            }

            glClearColor(0.02f, 0.02f, 0.02f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            ourShader.use();
            ourShader.setVec3("viewPos", camera.Position);

            // Ajustar proyección por si cambia tamaño ventana
            glfwGetFramebufferSize(window, &width, &height);
            SCR_WIDTH = width;
            SCR_HEIGHT = height;
            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
            glm::mat4 view = camera.GetViewMatrix();
            ourShader.setMat4("projection", projection);
            ourShader.setMat4("view", view);

            float intensidadFlicker = HorrorFlicker(currentFrame);

            // -------------------------------------------------------------------------------------
            // DETECTORES DE JUMPSCARE
            // -------------------------------------------------------------------------------------
            float distanciaAlPayaso = glm::distance(camera.Position, clownPosition);
            float distanciaAChuky = glm::distance(camera.Position, chukyPosition);

            float dynamicClownScale = clownScale;
            float dynamicChukyScale = chukyScale;
            bool jumpscarePayaso = false;
            bool jumpscareChuky = false;

            if (distanciaAlPayaso < 1.1f)
            {
                jumpscarePayaso = true;
                camera.Front = glm::normalize(clownPosition - camera.Position);
                dynamicClownScale = clownScale * 1.7f;
            }

            if (distanciaAChuky < 1.0f)
            {
                jumpscareChuky = true;
                camera.Front = glm::normalize(chukyPosition - camera.Position);
                dynamicChukyScale = chukyScale * 1.4f;
            }

            bool jumpscareActivo = jumpscarePayaso || jumpscareChuky;

            if (jumpscareActivo)
                intensidadFlicker = (sin(currentFrame * 90.0f) > 0.0f) ? 0.0f : 4.0f;

            // -------------------------------------------------------------------------------------
            // REVELACIÓN GRADUAL
            // -------------------------------------------------------------------------------------
            float revealPayaso = RevealFactor(tiempoDeJuego, CLOWN_APPEAR_TIME, REVEAL_FADE_DURATION);
            float revealChuky = RevealFactor(tiempoDeJuego, CHUKY_APPEAR_TIME, REVEAL_FADE_DURATION);
            float revealHacha = RevealFactor(tiempoDeJuego, HACHA_APPEAR_TIME, REVEAL_FADE_DURATION);
            float revealDanger = RevealFactor(tiempoDeJuego, DANGER_APPEAR_TIME, REVEAL_FADE_DURATION);
            float revealMascara = RevealFactor(tiempoDeJuego, MASCARA_APPEAR_TIME, REVEAL_FADE_DURATION);

            dynamicClownScale *= revealPayaso;
            dynamicChukyScale *= revealChuky;

            if (juegoIniciado)
                ClampPlayerToScene(revealPayaso, revealChuky, revealHacha, revealDanger, revealMascara);

            // PointLight (foco del techo)
            ourShader.setVec3("pointLight.position", glm::vec3(0.0f, 4.0f, 0.0f));
            ourShader.setFloat("pointLight.constant", 1.0f);
            ourShader.setFloat("pointLight.linear", 0.07f);
            ourShader.setFloat("pointLight.quadratic", 0.017f);
            ourShader.setVec3("pointLight.ambient", colorLuzTecho * 0.12f * intensidadFlicker);
            ourShader.setVec3("pointLight.diffuse", colorLuzTecho * 0.9f * intensidadFlicker);
            ourShader.setVec3("pointLight.specular", colorLuzTecho * 0.9f * intensidadFlicker);

            // SpotLight (linterna)
            glm::vec3 dynamicLinternaPos = camera.Position;
            if (isMoving)
                dynamicLinternaPos.x += cos(bobbingTimer) * 0.02f;

            ourShader.setVec3("spotLight.position", dynamicLinternaPos);
            ourShader.setVec3("spotLight.direction", camera.Front);
            ourShader.setFloat("spotLight.constant", 1.0f);
            ourShader.setFloat("spotLight.linear", 0.045f);
            ourShader.setFloat("spotLight.quadratic", 0.012f);
            ourShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(14.5f)));
            ourShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(20.0f)));

            if (flashlightOn)
            {
                if (jumpscareActivo) {
                    ourShader.setVec3("spotLight.diffuse", glm::vec3(3.0f, 0.0f, 0.0f));
                    ourShader.setVec3("spotLight.specular", glm::vec3(3.0f, 0.0f, 0.0f));
                }
                else {
                    ourShader.setVec3("spotLight.diffuse", glm::vec3(2.2f, 2.2f, 1.9f));
                    ourShader.setVec3("spotLight.specular", glm::vec3(2.2f, 2.2f, 1.9f));
                }
                ourShader.setVec3("spotLight.ambient", glm::vec3(0.0f));
            }
            else
            {
                ourShader.setVec3("spotLight.ambient", glm::vec3(0.0f));
                ourShader.setVec3("spotLight.diffuse", glm::vec3(0.0f));
                ourShader.setVec3("spotLight.specular", glm::vec3(0.0f));
            }

            // ---- GARAJE ----
            glm::mat4 modelMat = glm::mat4(1.0f);
            modelMat = glm::translate(modelMat, glm::vec3(0.0f, 0.0f, 0.0f));
            ourShader.setMat4("model", modelMat);
            garageModel.Draw(ourShader);

            // ---- PAYASO ----
            glm::mat4 clownMat = glm::mat4(1.0f);

            glm::vec3 clownFinalPos = clownPosition;
            clownFinalPos.y += CLOWN_FEET_OFFSET * clownScale;
            clownFinalPos.y += sin(currentFrame * 2.0f) * 0.08f;

            float factorTemblorPayaso = jumpscarePayaso ? 3.0f : 1.0f;
            clownFinalPos.x += distribution(generator) * factorTemblorPayaso;
            clownFinalPos.z += distribution(generator) * factorTemblorPayaso;

            clownMat = glm::translate(clownMat, clownFinalPos);

            glm::vec3 targetDirPayaso = glm::normalize(camera.Position - clownPosition);
            float anglePayaso = atan2(targetDirPayaso.x, targetDirPayaso.z);
            const float CLOWN_ROTATION_OFFSET = glm::radians(-50.0f);
            anglePayaso += CLOWN_ROTATION_OFFSET;

            clownMat = glm::rotate(clownMat, anglePayaso, glm::vec3(0.0f, 1.0f, 0.0f));
            clownMat = glm::scale(clownMat, glm::vec3(dynamicClownScale));

            ourShader.setMat4("model", clownMat);
            clownModel.Draw(ourShader);

            // ---- CHUKY ----
            glm::mat4 chukyMat = glm::mat4(1.0f);

            glm::vec3 chukyFinalPos = chukyPosition;
            chukyFinalPos.y += CHUKY_FEET_OFFSET * chukyScale;
            chukyFinalPos.y += (jumpscareChuky ? sin(currentFrame * 25.0f) * 0.02f : 0.0f);

            float factorTemblorChuky = jumpscareChuky ? 4.0f : 0.4f;
            chukyFinalPos.x += distribution(generator) * factorTemblorChuky;
            chukyFinalPos.z += distribution(generator) * factorTemblorChuky;

            chukyMat = glm::translate(chukyMat, chukyFinalPos);

            glm::vec3 targetDirChuky = glm::normalize(camera.Position - chukyPosition);
            float angleChuky = atan2(targetDirChuky.x, targetDirChuky.z);
            const float CHUKY_ROTATION_OFFSET = glm::radians(0.70f);
            angleChuky += CHUKY_ROTATION_OFFSET;

            chukyMat = glm::rotate(chukyMat, angleChuky, glm::vec3(0.0f, 1.0f, 0.0f));
            chukyMat = glm::scale(chukyMat, glm::vec3(dynamicChukyScale));

            ourShader.setMat4("model", chukyMat);
            chukyModel.Draw(ourShader);

            // ---- HACHA ----
            glm::mat4 hachaMat = glm::mat4(1.0f);
            hachaMat = glm::translate(hachaMat, hachaPosition);
            hachaMat = glm::rotate(hachaMat, hachaRotationY, glm::vec3(1.0f, 0.0f, 0.0f));
            hachaMat = glm::scale(hachaMat, glm::vec3(hachaScale * revealHacha));
            ourShader.setMat4("model", hachaMat);
            hachaModel.Draw(ourShader);

            // ---- CINTA DE PELIGRO ----
            glm::mat4 dangerMat = glm::mat4(1.0f);
            dangerMat = glm::translate(dangerMat, dangerPosition);
            dangerMat = glm::rotate(dangerMat, dangerRotationY, glm::vec3(0.0f, 1.0f, 0.0f));
            dangerMat = glm::scale(dangerMat, glm::vec3(dangerScale * revealDanger));
            ourShader.setMat4("model", dangerMat);
            dangerModel.Draw(ourShader);

            // ---- MÁSCARA ----
            glm::mat4 mascaraMat = glm::mat4(1.0f);
            mascaraMat = glm::translate(mascaraMat, mascaraPosition);
            mascaraMat = glm::rotate(mascaraMat, mascaraRotationY, glm::vec3(0.0f, 1.0f, 0.0f));
            mascaraMat = glm::scale(mascaraMat, glm::vec3(mascaraScale * revealMascara));
            ourShader.setMat4("model", mascaraMat);
            mascaraModel.Draw(ourShader);

            // -------------------------------------------------------------------------------------
            // HUD (siempre al final, antes de SwapBuffers)
            // -------------------------------------------------------------------------------------
            std::string estadoLinterna = flashlightOn ? "Linterna: ON" : "Linterna: OFF";
            textRenderer->RenderText(estadoLinterna, 10.0f, SCR_HEIGHT - 30.0f, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f));

            if (showMenu && juegoIniciado)
            {
                float startY = (float)SCR_HEIGHT - 60.0f;
                float stepY = 26.0f;
                textRenderer->RenderText("W: Al frente", 10.0f, startY, 0.4f, glm::vec3(1.0f, 1.0f, 1.0f));
                textRenderer->RenderText("S: Atras", 10.0f, startY - stepY, 0.4f, glm::vec3(1.0f, 1.0f, 1.0f));
                textRenderer->RenderText("A: Izquierda", 10.0f, startY - 2 * stepY, 0.4f, glm::vec3(1.0f, 1.0f, 1.0f));
                textRenderer->RenderText("D: Derecha", 10.0f, startY - 3 * stepY, 0.4f, glm::vec3(1.0f, 1.0f, 1.0f));
                textRenderer->RenderText("F: Prender/apagar linterna", 10.0f, startY - 4 * stepY, 0.4f, glm::vec3(1.0f, 1.0f, 1.0f));
                textRenderer->RenderText("M: Ocultar/mostrar Menu", 10.0f, startY - 5 * stepY, 0.4f, glm::vec3(1.0f, 1.0f, 1.0f));
                textRenderer->RenderText("H: Volver al Pasillo", 10.0f, startY - 6 * stepY, 0.4f, glm::vec3(0.9f, 0.2f, 0.2f));

                std::string volText = "Volumen: " + std::to_string(get_global_volume()) + "% (Flecha Arriba/Abajo)";
                textRenderer->RenderText(volText, 10.0f, startY - 7 * stepY, 0.4f, glm::vec3(0.0f, 1.0f, 0.5f));
            }

            if (!juegoIniciado)
            {
                std::string linea1 = "Bienvenido";
                std::string linea2 = "Intenta no asustarte.";
                std::string linea3 = "Presiona ENTER para continuar.";

                float scale1 = 0.7f;
                float scale2 = 0.6f;
                float scale3 = 0.6f;

                float ancho1 = textRenderer->GetTextWidth(linea1, scale1);
                float ancho2 = textRenderer->GetTextWidth(linea2, scale2);
                float ancho3 = textRenderer->GetTextWidth(linea3, scale3);

                // Alternativa segura a std::max para evitar problemas con macros en Windows
                float tempMax = (ancho1 > ancho2) ? ancho1 : ancho2;
                float anchoMaximo = (tempMax > ancho3) ? tempMax : ancho3;

                float paddingX = 40.0f;
                float paddingY = 30.0f;
                float cajaAncho = anchoMaximo + paddingX * 2.0f;
                float cajaAlto = 150.0f + paddingY;
                float cajaX = (SCR_WIDTH - cajaAncho) / 2.0f;
                float cajaY = SCR_HEIGHT / 2.0f - 55.0f;

                textRenderer->RenderQuad(cajaX, cajaY, cajaAncho, cajaAlto, glm::vec3(0.0f, 0.0f, 0.0f), 0.85f);

                textRenderer->RenderText(linea1, (SCR_WIDTH - ancho1) / 2.0f, SCR_HEIGHT / 2.0f + 60.0f, scale1, glm::vec3(0.85f, 0.0f, 0.0f));
                textRenderer->RenderText(linea2, (SCR_WIDTH - ancho2) / 2.0f, SCR_HEIGHT / 2.0f + 15.0f, scale2, glm::vec3(0.85f, 0.0f, 0.0f));

                float parpadeo = (sin(currentFrame * 4.0f) > 0.0f) ? 1.0f : 0.3f;
                textRenderer->RenderText(linea3, (SCR_WIDTH - ancho3) / 2.0f, SCR_HEIGHT / 2.0f - 40.0f, scale3, glm::vec3(0.4f, 0.0f, 0.0f) * parpadeo);
            }

            if (jumpscareActivo)
            {
                std::string msg = "!!!";
                float ancho = textRenderer->GetTextWidth(msg, 1.5f);
                textRenderer->RenderText(msg, (SCR_WIDTH - ancho) / 2.0f, SCR_HEIGHT / 2.0f, 1.5f, glm::vec3(1.0f, 0.0f, 0.0f));
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        haltmusic();
        delete textRenderer;
        textRenderer = nullptr;
    }

    float HorrorFlicker(float time)
    {
        float baseIntensity = 0.85f;
        float cycleTime = fmod(time, 8.0f);

        if (cycleTime > 2.0f && cycleTime < 2.3f)
            return (sin(time * 50.0f) > 0.0f) ? 0.15f : baseIntensity;
        if (cycleTime > 5.5f && cycleTime < 5.7f)
            return 0.05f;
        return baseIntensity;
    }

    // 0.0 antes de appearTime, crece con smoothstep hasta 1.0 durante fadeDuration
    float RevealFactor(float tiempoActual, float appearTime, float fadeDuration)
    {
        float t = glm::clamp((tiempoActual - appearTime) / fadeDuration, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    // Empuje 2D en XZ para que el jugador no atraviese paredes ni props ya revelados
    void ClampPlayerToScene(float revealPayaso, float revealChuky, float revealHacha, float revealDanger, float revealMascara)
    {
        camera.Position.x = glm::clamp(camera.Position.x, sceneMinX, sceneMaxX);
        camera.Position.z = glm::clamp(camera.Position.z, sceneMinZ, sceneMaxZ);

        auto empujarFuera = [&](const glm::vec3& objPos, float radio)
            {
                glm::vec2 playerXZ(camera.Position.x, camera.Position.z);
                glm::vec2 objXZ(objPos.x, objPos.z);
                float dist = glm::distance(playerXZ, objXZ);

                if (dist < radio && dist > 0.0001f)
                {
                    glm::vec2 direccion = glm::normalize(playerXZ - objXZ);
                    camera.Position.x = objXZ.x + direccion.x * radio;
                    camera.Position.z = objXZ.y + direccion.y * radio;
                }
            };

        if (revealPayaso > 0.05f)  empujarFuera(clownPosition, CLOWN_COLLISION_RADIUS);
        if (revealChuky > 0.05f)   empujarFuera(chukyPosition, CHUKY_COLLISION_RADIUS);
        if (revealHacha > 0.05f)   empujarFuera(hachaPosition, HACHA_COLLISION_RADIUS);
        if (revealDanger > 0.05f)  empujarFuera(dangerPosition, DANGER_COLLISION_RADIUS);
        if (revealMascara > 0.05f) empujarFuera(mascaraPosition, MASCARA_COLLISION_RADIUS);
    }

    void processInput(GLFWwindow* window)
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Tecla para volver al Pasillo principal
        if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS)
            g_NextScene = SCENE_PASILLO;

        bool enterPressed = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;
        if (enterPressed && !enterPressedLastFrame && !juegoIniciado)
            juegoIniciado = true;
        enterPressedLastFrame = enterPressed;

        bool upPressed = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
        if (upPressed && !upKeyPressedLastFrame)
            setvolume(get_global_volume() + 10);
        upKeyPressedLastFrame = upPressed;

        bool downPressed = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
        if (downPressed && !downKeyPressedLastFrame)
            setvolume(get_global_volume() - 10);
        downKeyPressedLastFrame = downPressed;

        bool mPressed = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
        if (mPressed && !mKeyPressedLastFrame)
            showMenu = !showMenu;
        mKeyPressedLastFrame = mPressed;

        if (!juegoIniciado)
            return;

        isMoving = false;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            camera.ProcessKeyboard(FORWARD, deltaTime);
            isMoving = true;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            camera.ProcessKeyboard(BACKWARD, deltaTime);
            isMoving = true;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            camera.ProcessKeyboard(LEFT, deltaTime);
            isMoving = true;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            camera.ProcessKeyboard(RIGHT, deltaTime);
            isMoving = true;
        }

        bool fPressed = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
        if (fPressed && !fKeyPressedLastFrame)
            flashlightOn = !flashlightOn;
        fKeyPressedLastFrame = fPressed;

        // Tecla P: imprime posición de cámara
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

        camera.ProcessMouseMovement(xoffset, yoffset);
    }
} // namespace Anahi