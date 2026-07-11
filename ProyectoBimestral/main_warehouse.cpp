// ============================================================
// main.cpp - Adaptado para cargar el modelo del Warehouse
// Con detección de colisiones (AABB), escala automática a un
// tamaño "realista" y cámara inicializada DENTRO del modelo,
// a la altura del piso (no encima del edificio).
// ============================================================

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>
#define STB_IMAGE_IMPLEMENTATION
#include <learnopengl/stb_image.h>

#include <iostream>
#include <algorithm>
#include <vector>
#include <cfloat>

// ---------------------------------------------------------
// Callbacks / funciones auxiliares
// ---------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

// ---------------------------------------------------------
// Configuración de ventana
// ---------------------------------------------------------
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// ---------------------------------------------------------
// Configuración de escala / jugador
// ---------------------------------------------------------
// Tamaño horizontal deseado del edificio en unidades de mundo.
// Si 1 unidad ~ 1 metro, 40 unidades es un warehouse grande.
const float TARGET_BUILDING_SIZE = 40.0f;

// Altura de los ojos del jugador sobre el piso (en unidades de mundo)
const float PLAYER_EYE_HEIGHT = 1.7f;

// Velocidad de caminata (unidades de mundo por segundo).
// Con TARGET_BUILDING_SIZE = 40 y esto en 3.0f, cruzar el
// edificio de punta a punta toma ~13 segundos caminando derecho.
const float PLAYER_WALK_SPEED = 3.0f;

// Radio "de cuerpo" del jugador para colisiones
const float PLAYER_RADIUS = 0.3f;

// Si tu modelo se ve "acostado" (viene en Z-up en vez de Y-up),
// poné esto en true para corregirlo automáticamente.
const bool MODEL_IS_Z_UP = false;

// ---------------------------------------------------------
// Cámara (posición real se calcula en main() según el AABB)
// ---------------------------------------------------------
Camera camera(glm::vec3(0.0f, 2.0f, 8.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// ---------------------------------------------------------
// Timing
// ---------------------------------------------------------
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// ---------------------------------------------------------
// Colisiones (AABB)
// ---------------------------------------------------------
struct AABB {
    glm::vec3 min;
    glm::vec3 max;
};

AABB g_worldAABB;
std::vector<AABB> collisionWalls; // paredes/columnas internas, se llenan a mano

AABB ComputeModelLocalAABB(Model& model)
{
    glm::vec3 vmin(FLT_MAX), vmax(-FLT_MAX);
    for (auto& mesh : model.meshes) {
        for (auto& v : mesh.vertices) {
            vmin = glm::min(vmin, v.Position);
            vmax = glm::max(vmax, v.Position);
        }
    }
    return { vmin, vmax };
}

AABB TransformAABB(const AABB& local, const glm::mat4& model)
{
    glm::vec3 corners[8] = {
        {local.min.x, local.min.y, local.min.z},
        {local.max.x, local.min.y, local.min.z},
        {local.min.x, local.max.y, local.min.z},
        {local.min.x, local.min.y, local.max.z},
        {local.max.x, local.max.y, local.min.z},
        {local.max.x, local.min.y, local.max.z},
        {local.min.x, local.max.y, local.max.z},
        {local.max.x, local.max.y, local.max.z},
    };
    glm::vec3 vmin(FLT_MAX), vmax(-FLT_MAX);
    for (auto& c : corners) {
        glm::vec3 wc = glm::vec3(model * glm::vec4(c, 1.0f));
        vmin = glm::min(vmin, wc);
        vmax = glm::max(vmax, wc);
    }
    return { vmin, vmax };
}

bool CollidesWithAABB(const glm::vec3& pos, const AABB& box, float radius)
{
    return (pos.x + radius > box.min.x && pos.x - radius < box.max.x &&
        pos.y + radius > box.min.y && pos.y - radius < box.max.y &&
        pos.z + radius > box.min.z && pos.z - radius < box.max.z);
}

glm::vec3 TryMove(const glm::vec3& currentPos, const glm::vec3& delta, const AABB& worldBounds)
{
    glm::vec3 newPos = currentPos;

    glm::vec3 testX = newPos + glm::vec3(delta.x, 0.0f, 0.0f);
    bool blockedX = false;
    for (auto& w : collisionWalls) {
        if (CollidesWithAABB(testX, w, PLAYER_RADIUS)) { blockedX = true; break; }
    }
    if (!blockedX) newPos.x = testX.x;

    glm::vec3 testZ = newPos + glm::vec3(0.0f, 0.0f, delta.z);
    bool blockedZ = false;
    for (auto& w : collisionWalls) {
        if (CollidesWithAABB(testZ, w, PLAYER_RADIUS)) { blockedZ = true; break; }
    }
    if (!blockedZ) newPos.z = testZ.z;

    newPos.x = std::clamp(newPos.x, worldBounds.min.x + PLAYER_RADIUS, worldBounds.max.x - PLAYER_RADIUS);
    newPos.z = std::clamp(newPos.z, worldBounds.min.z + PLAYER_RADIUS, worldBounds.max.z - PLAYER_RADIUS);

    return newPos;
}

int main()
{
    // -------------------- Inicialización GLFW --------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Warehouse - Proyecto Bimestral", NULL, NULL);
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

    // -------------------- GLAD --------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // -------------------- Shaders --------------------
    Shader ourShader("shaders/model_loading.vs", "shaders/model_loading.fs");

    // -------------------- Carga del modelo --------------------
    Model ourModel("C:/Users/redin/source/repos/2026A_GR1SW_GR4/ProyectoBimestral/model/Warehouse/abandoned_warehouse_-_interior_scene/Sin_nombre.obj");

    // -------------------- AABB local (tamaño original del modelo) --------------------
    AABB localBox = ComputeModelLocalAABB(ourModel);
    glm::vec3 localSize = localBox.max - localBox.min;

    std::cout << "[DEBUG] Tamano local del modelo (sin escalar): "
        << localSize.x << " x " << localSize.y << " x " << localSize.z << std::endl;

    // -------------------- Escala automática --------------------
    // Escala uniforme para que la dimension horizontal mas grande
    // (X o Z) del modelo termine midiendo TARGET_BUILDING_SIZE.
    float horizontalExtent = std::max(localSize.x, localSize.z);
    float autoScale = (horizontalExtent > 0.0001f) ? (TARGET_BUILDING_SIZE / horizontalExtent) : 1.0f;

    std::cout << "[DEBUG] Escala automatica calculada: " << autoScale << std::endl;

    glm::mat4 warehouseModelMatrix = glm::mat4(1.0f);

    if (MODEL_IS_Z_UP)
    {
        // Corrige modelos exportados en Z-up (Blender/Sketchfab) a Y-up
        warehouseModelMatrix = glm::rotate(warehouseModelMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    }

    // Traslada para que el piso (min.y del modelo YA escalado) quede en y = 0
    warehouseModelMatrix = glm::translate(warehouseModelMatrix, glm::vec3(0.0f, -localBox.min.y * autoScale, 0.0f));
    warehouseModelMatrix = glm::scale(warehouseModelMatrix, glm::vec3(autoScale));

    // -------------------- AABB de mundo --------------------
    g_worldAABB = TransformAABB(localBox, warehouseModelMatrix);

    std::cout << "[DEBUG] AABB de mundo: min("
        << g_worldAABB.min.x << ", " << g_worldAABB.min.y << ", " << g_worldAABB.min.z
        << ") max(" << g_worldAABB.max.x << ", " << g_worldAABB.max.y << ", " << g_worldAABB.max.z
        << ")" << std::endl;

    // -------------------- Spawn de la cámara --------------------
    // Centro horizontal del edificio, a nivel de piso + altura de ojos.
    // Si el spawn cae dentro de una columna o maquinaria, movelo unas
    // unidades en X o Z hasta que quede en un pasillo libre.
    glm::vec3 center = (g_worldAABB.min + g_worldAABB.max) * 0.5f;
    camera.Position = glm::vec3(center.x, g_worldAABB.min.y + PLAYER_EYE_HEIGHT, center.z);
    camera.MovementSpeed = PLAYER_WALK_SPEED;

    std::cout << "[DEBUG] Camara inicial en: ("
        << camera.Position.x << ", " << camera.Position.y << ", " << camera.Position.z
        << ")" << std::endl;

    // -------------------- Paredes/columnas internas --------------------
    // Vacio por defecto. Jugá, topá con una pared, presioná F1 para
    // imprimir la posición actual de la cámara en consola, y agregá
    // la caja correspondiente aquí, por ejemplo:
    //
    // collisionWalls.push_back({ glm::vec3(-2.0f, 0.0f, -5.0f), glm::vec3(2.0f, 3.0f, -4.5f) });

    // -------------------- Render loop --------------------
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 500.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);
        ourShader.setMat4("model", warehouseModelMatrix);

        ourModel.Draw(ourShader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

// ---------------------------------------------------------
// Input
// ---------------------------------------------------------
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Tecla de debug: imprime la posicion actual de la camara.
    // Util para calibrar collisionWalls a mano.
    static bool f1WasPressed = false;
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS)
    {
        if (!f1WasPressed)
        {
            std::cout << "[DEBUG] camera.Position = ("
                << camera.Position.x << ", " << camera.Position.y << ", " << camera.Position.z
                << ")" << std::endl;
        }
        f1WasPressed = true;
    }
    else
    {
        f1WasPressed = false;
    }

    float velocity = camera.MovementSpeed * deltaTime;
    glm::vec3 delta(0.0f);

    glm::vec3 front = glm::normalize(glm::vec3(camera.Front.x, 0.0f, camera.Front.z));
    glm::vec3 right = glm::normalize(glm::cross(front, camera.Up));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) delta += front * velocity;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) delta -= front * velocity;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) delta -= right * velocity;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) delta += right * velocity;

    camera.Position = TryMove(camera.Position, delta, g_worldAABB);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
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

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}