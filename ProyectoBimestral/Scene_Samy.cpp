// =====================================================================================
//  ESCENA DE TERROR F1 - GARAGE (grande) + AUTO ESTATICO + PERSONAJE + LUCES
//  Shaders: shader_garage_horror.vs / shader_garage_horror.fs
// =====================================================================================

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
// Posición real encontrada dentro del garage (con el modo debug + tecla P).
Camera camera(glm::vec3(0.695734f, -0.132227f, 0.389627f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// -------------------------------------------------------------------------------------
// ESCENA / LUCES
// -------------------------------------------------------------------------------------

// Escala del garage: como pediste que sea GRANDE tipo escenario de juego,
// empieza aquí. Si tu modelo viene en metros reales y ya es grande, deja 1.0f;
// si viene pequeño (típico de asset packs), sube este valor (2.0f, 5.0f, etc.)
float garageScale = 1.0f;

// Posición del auto DENTRO del garage 

glm::vec3 carPosition = glm::vec3(0.28f, -1.5f, 0.55f);
float carRotationY = 0.0f;   // gira el auto si no está mirando hacia donde quieres
float carScale = 0.0005f;

// Luz blanca (antes "parpadeante", ahora tenue y casi constante)
glm::vec3 flickerLightPos = glm::vec3(5.0f, 3.0f, 2.0f);
glm::vec3 flickerLightColor = glm::vec3(1.0f, 1.0f, 0.95f);

// Luz ambiente tenue y pareja (para que no sea 100% negro pero siga siendo oscuro).
// No viene de una dirección, así que no genera sombras ni "foco" — solo un piso
// mínimo de visibilidad en toda la escena.
glm::vec3 ambientLightColor = glm::vec3(0.04f, 0.04f, 0.045f); // ligero tinte azulado, típico de horror

// -------------------------------------------------------------------------------------
// COLISIONES / LIMITES DEL GARAGE (caja invisible para no atravesar paredes)
// -------------------------------------------------------------------------------------

float garageMinX = -0.744853f;
float garageMaxX = 0.695734f;
float garageMinZ = -1.03401f;
float garageMaxZ = 1.00179f;
float playerHeight = -0.1375f; // promedio de las alturas de piso medidas

// Linterna del personaje
bool flashlightOn = true;
bool fKeyPressedLastFrame = false;


bool debugMode = false;
bool pKeyPressedLastFrame = false;

int main()
{
    // glfw: initialize and configure
    // ------------------------------
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

    //stbi_set_flip_vertically_on_load(true); // prueba con/sin esto si las texturas se ven mal

    glEnable(GL_DEPTH_TEST);

    // shader con iluminación (dirLight apagada, 2 pointLights activas, spotLight = linterna)
    Shader ourShader("shaders/vertex.vs", "shaders/fragment.fs");

    // ---- CARGA DE MODELOS ----
    // Ajusta el nombre del archivo .obj al que realmente tengas en cada carpeta
    Model garageModel("./model/garage/garage.obj");
    Model carModel("./model/car/car.obj");

    // El modelo es muy pequeño (cabe en ~1-2 unidades), así que la velocidad tiene
    // que ser proporcionalmente chica o vas a atravesar todo el garage de un tirón.
    camera.MovementSpeed = debugMode ? 0.5f : 0.2f;

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        float time = currentFrame;
        float flickerIntensity = HorrorFlicker(time);

        // --- Color de fondo: gris claro en modo debug (para distinguir fondo de
        //     geometría), negro total en modo terror ---
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

        // --- Directional light: usada SOLO como luz ambiente pareja (sin dirección real,
        //     diffuse/specular en 0 para que no cree un "sol" ni sombreado direccional) ---
        ourShader.setVec3("dirLight.direction", glm::vec3(-0.2f, -1.0f, -0.3f));
        ourShader.setVec3("dirLight.ambient", ambientLightColor);
        ourShader.setVec3("dirLight.diffuse", glm::vec3(0.0f));
        ourShader.setVec3("dirLight.specular", glm::vec3(0.0f));

        // --- pointLights[0]: luz blanca tenue (antes parpadeaba caótico, ahora
        //     casi constante — ver HorrorFlicker() más abajo) ---
        ourShader.setVec3("pointLights[0].position", flickerLightPos);
        ourShader.setVec3("pointLights[0].ambient", flickerLightColor * 0.02f * flickerIntensity);
        ourShader.setVec3("pointLights[0].diffuse", flickerLightColor * flickerIntensity);
        ourShader.setVec3("pointLights[0].specular", flickerLightColor * flickerIntensity);
        ourShader.setFloat("pointLights[0].constant", 1.0f);
        ourShader.setFloat("pointLights[0].linear", 0.09f);
        ourShader.setFloat("pointLights[0].quadratic", 0.032f);

        // --- pointLights[1], [2] y [3]: apagadas (reservadas para más adelante) ---
        for (int i = 1; i < 4; i++)
        {
            std::string base = "pointLights[" + std::to_string(i) + "].";
            ourShader.setVec3(base + "position", glm::vec3(0.0f));
            ourShader.setVec3(base + "ambient", glm::vec3(0.01f));
            ourShader.setVec3(base + "diffuse", glm::vec3(0.0f));
            ourShader.setVec3(base + "specular", glm::vec3(0.0f));
            ourShader.setFloat(base + "constant", 1.0f);
            ourShader.setFloat(base + "linear", 0.09f);
            ourShader.setFloat(base + "quadratic", 0.032f);
        }

        // --- spotLight: LA LINTERNA DEL PERSONAJE ---
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

        // ---------------------------------------------------------------
        // 1) EL GARAGE (la sala completa donde te mueves, GRANDE)
        // ---------------------------------------------------------------
        glm::mat4 garageMat = glm::mat4(1.0f);
        garageMat = glm::translate(garageMat, glm::vec3(0.0f, 0.0f, 0.0f));
        garageMat = glm::scale(garageMat, glm::vec3(garageScale));
        ourShader.setMat4("model", garageMat);
        garageModel.Draw(ourShader);

        // ---------------------------------------------------------------
        // 2) EL AUTO (estático, dentro del garage, achicado para que quepa)
        // ---------------------------------------------------------------
        glm::mat4 carMat = glm::mat4(1.0f);
        carMat = glm::translate(carMat, carPosition);
        carMat = glm::rotate(carMat, glm::radians(carRotationY), glm::vec3(0.0f, 1.0f, 0.0f));
        carMat = glm::scale(carMat, glm::vec3(carScale));
        ourShader.setMat4("model", carMat);
        carModel.Draw(ourShader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

// -------------------------------------------------------------------------------------
// Evita que el jugador salga de la caja invisible del garage (colisión simple con
// paredes rectas) y lo mantiene siempre a la misma altura (sin volar al mirar
// arriba/abajo, ya que camera.ProcessKeyboard mueve la cámara sobre el vector Front,
// que incluye componente vertical cuando hay pitch).
// -------------------------------------------------------------------------------------
void ClampPlayerToGarage()
{
    camera.Position.x = glm::clamp(camera.Position.x, garageMinX, garageMaxX);
    camera.Position.z = glm::clamp(camera.Position.z, garageMinZ, garageMaxZ);
    camera.Position.y = playerHeight;
}

// -------------------------------------------------------------------------------------
// Parpadeo de luces
// -------------------------------------------------------------------------------------
float HorrorFlicker(float time)
{
    // ---- CÓDIGO ORIGINAL: parpadeo caótico tipo tubo fluorescente casi muerto ----
    // Comentado por ahora para poder ver bien la escena mientras se ajustan
    // límites y posición del auto. Descomenta esto (y comenta el bloque de abajo)
    // cuando quieras volver al efecto terror completo.
    
    float base = sin(time * 30.0f) * sin(time * 13.7f);
    float intensity = (base > 0.2f) ? 1.0f : 0.05f;

    int cycle = (int)(time * 10.0f);
    static int lastCycle = -1;
    static bool blackout = false;
    if (cycle != lastCycle)
    {
        lastCycle = cycle;
        blackout = (rand() % 100) < 8; // ~8% de apagón total por ciclo de 0.1s
    }
    if (blackout) intensity = 0.0f;

    return intensity;
    

    // ---- NUEVO: luz tenue y casi constante, con un titileo corto cada 40s ----
    float baseIntensity = 0.35f;          // suficiente para ver la sala, pero tenue
    float cycleTime = fmod(time, 180.0f);  // se reinicia cada 40 segundos

    if (cycleTime < 0.6f)
    {
        // Ventana de 0.6s cada 40s donde la luz titila rápido antes de
        // volver a su brillo tenue normal
        float flicker = sin(cycleTime * 60.0f);
        return (flicker > 0.0f) ? baseIntensity * 0.15f : baseIntensity;
    }

    return baseIntensity;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame
// -------------------------------------------------------------------------------------
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
        // Vuelo libre en modo debug: Space sube, Ctrl izquierdo baja.
        // Así puedes buscar a qué altura Y está el piso real del garage.
        float flySpeed = camera.MovementSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            camera.Position.y += flySpeed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
            camera.Position.y -= flySpeed;

        // Imprime tu posición en consola solo cuando presionas P (una vez por
        // pulsación, no mientras la mantienes apretada)
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
        // No dejar que el jugador atraviese las paredes ni "vuele" al mirar arriba/abajo
        ClampPlayerToGarage();
    }

    // Toggle de linterna con F (tecla F: prende / apaga la linterna) (solo reacciona al momento de presionar, no mientras se mantiene)
    bool fPressed = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    if (fPressed && !fKeyPressedLastFrame)
        flashlightOn = !flashlightOn;
    fKeyPressedLastFrame = fPressed;
}

// glfw: whenever the window size changed (by OS or user resize) this callback executes
// -------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
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

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll((float)yoffset);
}