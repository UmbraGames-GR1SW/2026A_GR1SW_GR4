#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>
#include <cmath>
#include <random> // Para el efecto de temblor inquietante
#include <string>
#include <algorithm>

#include "text_renderer.h" // <-- HUD de texto 2D

#define STB_IMAGE_IMPLEMENTATION
#include "learnopengl/stb_image.h" 

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void processInput(GLFWwindow* window);
float HorrorFlicker(float time);
float RevealFactor(float tiempoActual, float appearTime, float fadeDuration);

// Dimensiones de ventana
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// =====================================================================================
// CONFIGURACIÓN DE CÁMARA Y DINÁMICAS DEL JUGADOR
// =====================================================================================
Camera camera(glm::vec3(0.0f, 1.2f, 4.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Variables para el cabeceo de la cámara (Head Bobbing)
float bobbingTimer = 0.0f;
bool isMoving = false;

// Tiempos
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Estado de la linterna
bool flashlightOn = true;
bool fKeyPressedLastFrame = false;

// -----------------------------------------------------------------------------
// Estado de la pantalla de bienvenida
// -----------------------------------------------------------------------------
bool juegoIniciado = false;
bool enterPressedLastFrame = false;

// Configuración de iluminación ambiental
glm::vec3 colorLuzTecho = glm::vec3(0.75f, 0.75f, 0.7f);

// =====================================================================================
// HUD: RENDERER DE TEXTO 2D
// =====================================================================================
TextRenderer* textRenderer = nullptr;

// =====================================================================================
// VARIABLES GLOBALES - COORDENADAS ESTÁTICAS DEL PAYASO
// =====================================================================================
glm::vec3 clownPosition = glm::vec3(0.0f, 0.0f, -2.0f);
float clownScale = 0.0038f;
const float CLOWN_FEET_OFFSET = 8.926056f;

// =====================================================================================
// VARIABLES GLOBALES - COORDENADAS ESTÁTICAS DE CHUKY
// =====================================================================================
glm::vec3 chukyPosition = glm::vec3(-6.0f, 0.0f, 1.8f);
float chukyScale = 1.0f;
const float CHUKY_FEET_OFFSET = 0.00636f;

// =====================================================================================
// VARIABLES GLOBALES - HACHA (PROP ESTÁTICO EN EL CENTRO DE LA HABITACIÓN)
// =====================================================================================
glm::vec3 hachaPosition = glm::vec3(4.5f, 0.0f, 0.0f);
float hachaScale = 4.0f;
float hachaRotationY = glm::radians(25.0f);

// =====================================================================================
// VARIABLES GLOBALES - CINTA DE PELIGRO (PROP ESTÁTICO BLOQUEANDO LA PUERTA)
// =====================================================================================
glm::vec3 dangerPosition = glm::vec3(-6.0f, 1.0f, -3.5f);
float dangerScale = 1.0f;
float dangerRotationY = glm::radians(50.0f);

// =====================================================================================
// VARIABLES GLOBALES - MÁSCARA (PROP ESTÁTICO DECORATIVO CONTRA LA PARED)
// =====================================================================================
glm::vec3 mascaraPosition = glm::vec3(-9.0f, 0.0f, 0.0f);
float mascaraScale = 1.0f;
float mascaraRotationY = glm::radians(90.0f);

// =====================================================================================
// TIEMPOS DE APARICIÓN GRADUAL (los modelos aparecen tras X segundos de haber
// empezado a jugar, sin importar la distancia a la que estés viéndolos)
// appearTime   = segundos desde que se presiona ENTER hasta que empieza a aparecer
// fadeDuration = cuántos segundos tarda en crecer de 0 a su tamaño completo
// =====================================================================================
const float CLOWN_APPEAR_TIME = 5.0f;
const float CHUKY_APPEAR_TIME = 10.0f;
const float HACHA_APPEAR_TIME = 3.0f;
const float DANGER_APPEAR_TIME = 7.0f;
const float MASCARA_APPEAR_TIME = 12.0f;
const float REVEAL_FADE_DURATION = 1.5f;

// Momento (en tiempo de glfwGetTime) en el que el jugador presionó ENTER
float tiempoInicioJuego = 0.0f;
bool tiempoInicioRegistrado = false;

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Backroom Garage Scene - Anahi", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    stbi_set_flip_vertically_on_load(false);

    // -----------------------------------------------------------------------------
    // Inicializar el HUD de texto (requiere contexto OpenGL + GLAD ya cargados)
    // -----------------------------------------------------------------------------
    textRenderer = new TextRenderer(SCR_WIDTH, SCR_HEIGHT);
    if (!textRenderer->Load("fonts/BlackOpsOne-Regular.ttf", 48)) // <-- ajusta la ruta a tu .ttf
    {
        std::cout << "ADVERTENCIA: No se pudo cargar la fuente del HUD" << std::endl;
    }

    Shader ourShader("shaders/Vertex_Anahi.vs", "shaders/Fragment_Anahi.fs");

    // Carga de modelos
    Model garageModel("./model/garage/garage.obj");
    Model clownModel("./model/payaso/payaso.obj");
    Model chukyModel("./model/chuky/chuky.obj");
    Model hachaModel("./model/hacha/hacha.obj");
    Model dangerModel("./model/danger/danger.obj");
    Model mascaraModel("./model/mascara/mascara.obj");

    camera.MovementSpeed = 2.5f;

    // Configuración para el generador de temblores lúgubres
    std::default_random_engine generator;
    std::uniform_real_distribution<float> distribution(-0.008f, 0.008f);

    // Render Loop
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // Registrar el instante exacto en que arranca el juego (justo tras ENTER)
        if (juegoIniciado && !tiempoInicioRegistrado)
        {
            tiempoInicioJuego = currentFrame;
            tiempoInicioRegistrado = true;
        }
        float tiempoDeJuego = juegoIniciado ? (currentFrame - tiempoInicioJuego) : 0.0f;

        // Lógica de Head Bobbing (cabeceo al caminar)
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

        // Fondo oscuro
        glClearColor(0.02f, 0.02f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();
        ourShader.setVec3("viewPos", camera.Position);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        float intensidadFlicker = HorrorFlicker(currentFrame);

        // -----------------------------------------------------------------------------
        // 🌟 LÓGICA DE VIDEOJUEGO: DETECTORES DE JUMPSCARE (DISTANCIA TRIDIMENSIONAL)
        // -----------------------------------------------------------------------------
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

        // Chuky tiene su propio radio de detección y su propio "salto" de escala
        if (distanciaAChuky < 1.0f)
        {
            jumpscareChuky = true;
            camera.Front = glm::normalize(chukyPosition - camera.Position);
            dynamicChukyScale = chukyScale * 1.4f;
        }

        // Si cualquiera de los dos está activo, la escena entra en modo pánico
        bool jumpscareActivo = jumpscarePayaso || jumpscareChuky;

        if (jumpscareActivo)
        {
            // Parpadeo caótico, histérico y ultra rápido
            intensidadFlicker = (sin(currentFrame * 90.0f) > 0.0f) ? 0.0f : 4.0f;
        }

        // -----------------------------------------------------------------------------
        // 🌟 REVELACIÓN GRADUAL: los modelos aparecen tras cierto tiempo de juego
        // -----------------------------------------------------------------------------
        float revealPayaso = RevealFactor(tiempoDeJuego, CLOWN_APPEAR_TIME, REVEAL_FADE_DURATION);
        float revealChuky = RevealFactor(tiempoDeJuego, CHUKY_APPEAR_TIME, REVEAL_FADE_DURATION);
        float revealHacha = RevealFactor(tiempoDeJuego, HACHA_APPEAR_TIME, REVEAL_FADE_DURATION);
        float revealDanger = RevealFactor(tiempoDeJuego, DANGER_APPEAR_TIME, REVEAL_FADE_DURATION);
        float revealMascara = RevealFactor(tiempoDeJuego, MASCARA_APPEAR_TIME, REVEAL_FADE_DURATION);

        dynamicClownScale *= revealPayaso;
        dynamicChukyScale *= revealChuky;

        // PointLight (Foco Fijo del Techo)
        ourShader.setVec3("pointLight.position", glm::vec3(0.0f, 4.0f, 0.0f));
        ourShader.setFloat("pointLight.constant", 1.0f);
        ourShader.setFloat("pointLight.linear", 0.07f);
        ourShader.setFloat("pointLight.quadratic", 0.017f);
        ourShader.setVec3("pointLight.ambient", colorLuzTecho * 0.12f * intensidadFlicker);
        ourShader.setVec3("pointLight.diffuse", colorLuzTecho * 0.9f * intensidadFlicker);
        ourShader.setVec3("pointLight.specular", colorLuzTecho * 0.9f * intensidadFlicker);

        // SpotLight (Linterna con desfase sutil por respiración)
        glm::vec3 dynamicLinternaPos = camera.Position;
        if (isMoving) {
            dynamicLinternaPos.x += cos(bobbingTimer) * 0.02f;
        }

        ourShader.setVec3("spotLight.position", dynamicLinternaPos);
        ourShader.setVec3("spotLight.direction", camera.Front);
        ourShader.setFloat("spotLight.constant", 1.0f);
        ourShader.setFloat("spotLight.linear", 0.045f);
        ourShader.setFloat("spotLight.quadratic", 0.012f);
        ourShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(14.5f)));
        ourShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(20.0f)));

        if (flashlightOn)
        {
            // Si cualquier jumpscare está activo, la linterna se descontrola en rojo de alarma
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

        // -----------------------------------------------------------------------------
        // DIBUJAR ESCENARIO: EL GARAJE
        // -----------------------------------------------------------------------------
        glm::mat4 modelMat = glm::mat4(1.0f);
        modelMat = glm::translate(modelMat, glm::vec3(0.0f, 0.0f, 0.0f));
        ourShader.setMat4("model", modelMat);
        garageModel.Draw(ourShader);

        // -----------------------------------------------------------------------------
        // DIBUJAR ENTIDAD: EL PAYASO CON SISTEMA DE SUSTO INTEGRADO
        // -----------------------------------------------------------------------------
        glm::mat4 clownMat = glm::mat4(1.0f);

        glm::vec3 clownFinalPos = clownPosition;
        clownFinalPos.y += CLOWN_FEET_OFFSET * clownScale;
        clownFinalPos.y += sin(currentFrame * 2.0f) * 0.08f; // levitación espectral

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

        // -----------------------------------------------------------------------------
        // DIBUJAR ENTIDAD: CHUKY CON SU PROPIO SISTEMA DE SUSTO
        // -----------------------------------------------------------------------------
        glm::mat4 chukyMat = glm::mat4(1.0f);

        glm::vec3 chukyFinalPos = chukyPosition;
        chukyFinalPos.y += CHUKY_FEET_OFFSET * chukyScale;
        // Chuky no levita: se arrastra/tiembla en el piso, más terrenal que el payaso
        chukyFinalPos.y += (jumpscareChuky ? sin(currentFrame * 25.0f) * 0.02f : 0.0f);

        float factorTemblorChuky = jumpscareChuky ? 4.0f : 0.4f; // ligero temblor incluso en reposo
        chukyFinalPos.x += distribution(generator) * factorTemblorChuky;
        chukyFinalPos.z += distribution(generator) * factorTemblorChuky;

        chukyMat = glm::translate(chukyMat, chukyFinalPos);

        // Chuky también gira para "mirarte" cuando estás cerca
        glm::vec3 targetDirChuky = glm::normalize(camera.Position - chukyPosition);
        float angleChuky = atan2(targetDirChuky.x, targetDirChuky.z);
        // Si tu modelo no queda de frente, ajusta este offset como se hizo con el payaso
        const float CHUKY_ROTATION_OFFSET = glm::radians(0.70f);
        angleChuky += CHUKY_ROTATION_OFFSET;

        chukyMat = glm::rotate(chukyMat, angleChuky, glm::vec3(0.0f, 1.0f, 0.0f));
        chukyMat = glm::scale(chukyMat, glm::vec3(dynamicChukyScale));

        ourShader.setMat4("model", chukyMat);
        chukyModel.Draw(ourShader);

        // -----------------------------------------------------------------------------
        // DIBUJAR PROP: EL HACHA (ESTÁTICA, EN EL CENTRO DE LA HABITACIÓN)
        // -----------------------------------------------------------------------------
        glm::mat4 hachaMat = glm::mat4(1.0f);
        hachaMat = glm::translate(hachaMat, hachaPosition);
        hachaMat = glm::rotate(hachaMat, hachaRotationY, glm::vec3(1.0f, 0.0f, 0.0f));
        hachaMat = glm::scale(hachaMat, glm::vec3(hachaScale * revealHacha));

        ourShader.setMat4("model", hachaMat);
        hachaModel.Draw(ourShader);

        // -----------------------------------------------------------------------------
        // DIBUJAR PROP: CINTA DE PELIGRO (ESTÁTICA, BLOQUEANDO LA PUERTA)
        // -----------------------------------------------------------------------------
        glm::mat4 dangerMat = glm::mat4(1.0f);
        dangerMat = glm::translate(dangerMat, dangerPosition);
        dangerMat = glm::rotate(dangerMat, dangerRotationY, glm::vec3(0.0f, 1.0f, 0.0f));
        dangerMat = glm::scale(dangerMat, glm::vec3(dangerScale * revealDanger));

        ourShader.setMat4("model", dangerMat);
        dangerModel.Draw(ourShader);

        // -----------------------------------------------------------------------------
        // DIBUJAR PROP: LA MÁSCARA (ESTÁTICA, DECORACIÓN DE PARED)
        // -----------------------------------------------------------------------------
        glm::mat4 mascaraMat = glm::mat4(1.0f);
        mascaraMat = glm::translate(mascaraMat, mascaraPosition);
        mascaraMat = glm::rotate(mascaraMat, mascaraRotationY, glm::vec3(0.0f, 1.0f, 0.0f));
        mascaraMat = glm::scale(mascaraMat, glm::vec3(mascaraScale * revealMascara));

        ourShader.setMat4("model", mascaraMat);
        mascaraModel.Draw(ourShader);

        // -----------------------------------------------------------------------------
        // HUD: TEXTO 2D ENCIMA DE LA ESCENA (siempre AL FINAL, antes de SwapBuffers)
        // -----------------------------------------------------------------------------
        std::string estadoLinterna = flashlightOn ? "Linterna: ON" : "Linterna: OFF";
        textRenderer->RenderText(estadoLinterna, 10.0f, SCR_HEIGHT - 30.0f, 0.5f, glm::vec3(1.0f, 1.0f, 1.0f));

        // Pantalla de bienvenida: se muestra hasta que el jugador presiona ENTER
        if (!juegoIniciado)
        {
            std::string linea1 = "Bienvenido a la sala mas miedosa";
            std::string linea2 = "Logra pasarla sin asustarte para salir";
            std::string linea3 = "Presiona ENTER para continuar";

            float scale1 = 0.7f;
            float scale2 = 0.6f;
            float scale3 = 0.6f;

            float ancho1 = textRenderer->GetTextWidth(linea1, scale1);
            float ancho2 = textRenderer->GetTextWidth(linea2, scale2);
            float ancho3 = textRenderer->GetTextWidth(linea3, scale3);

            // Fondo semi-transparente detrás de las 3 líneas para mejorar la lectura
            float anchoMaximo = std::max({ ancho1, ancho2, ancho3 });
            float paddingX = 40.0f;
            float paddingY = 30.0f;
            float cajaAncho = anchoMaximo + paddingX * 2.0f;
            float cajaAlto = 150.0f + paddingY;
            float cajaX = (SCR_WIDTH - cajaAncho) / 2.0f;
            float cajaY = SCR_HEIGHT / 2.0f - 55.0f;

            textRenderer->RenderQuad(cajaX, cajaY, cajaAncho, cajaAlto, glm::vec3(0.0f, 0.0f, 0.0f), 0.85f);

            textRenderer->RenderText(linea1, (SCR_WIDTH - ancho1) / 2.0f, SCR_HEIGHT / 2.0f + 60.0f, scale1, glm::vec3(0.85f, 0.0f, 0.0f));
            textRenderer->RenderText(linea2, (SCR_WIDTH - ancho2) / 2.0f, SCR_HEIGHT / 2.0f + 15.0f, scale2, glm::vec3(0.85f, 0.0f, 0.0f));

            // "Presiona ENTER" parpadea para llamar la atención (rojo mas oscuro)
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

    delete textRenderer;
    glfwTerminate();
    return 0;
}

float HorrorFlicker(float time)
{
    float baseIntensity = 0.85f;
    float cycleTime = fmod(time, 8.0f);

    if (cycleTime > 2.0f && cycleTime < 2.3f)
    {
        return (sin(time * 50.0f) > 0.0f) ? 0.15f : baseIntensity;
    }
    if (cycleTime > 5.5f && cycleTime < 5.7f)
    {
        return 0.05f;
    }
    return baseIntensity;
}

// Calcula qué tan "revelado" debe estar un modelo según el tiempo transcurrido
// desde que el jugador empezó a jugar (presionó ENTER). Devuelve 0.0 (invisible)
// antes de "appearTime", y crece suavemente (smoothstep) hasta 1.0 (tamaño
// completo) durante los "fadeDuration" segundos siguientes.
float RevealFactor(float tiempoActual, float appearTime, float fadeDuration)
{
    float t = glm::clamp((tiempoActual - appearTime) / fadeDuration, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t); // smoothstep
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Detectar ENTER para iniciar el juego (solo se activa una vez)
    bool enterPressed = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;
    if (enterPressed && !enterPressedLastFrame && !juegoIniciado)
        juegoIniciado = true;
    enterPressedLastFrame = enterPressed;

    // Si todavía no se presiona ENTER, no se procesa movimiento
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