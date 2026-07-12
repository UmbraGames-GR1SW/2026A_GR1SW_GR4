// ============================================================
// main.cpp - Adaptado para cargar el modelo del Warehouse
// Colisiones AUTOMATICAS por malla (paredes, puertas, sillas,
// columnas, etc.), escala automatica y camara dentro del piso.
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
const float TARGET_BUILDING_SIZE = 40.0f;  // ancho horizontal deseado del edificio
const float PLAYER_EYE_HEIGHT = 1.7f;
const float PLAYER_WALK_SPEED = 3.0f;
const float PLAYER_RADIUS = 0.3f;
const bool  MODEL_IS_Z_UP = false; // poner true si el modelo aparece "acostado"

// Umbral para decidir si una malla es "piso/techo" y por lo tanto
// NO debe bloquear el movimiento horizontal. Una malla se considera
// piso/techo si cubre casi todo el ancho/profundidad del edificio Y
// es muy delgada en altura.
const float FLOOR_CEILING_FOOTPRINT_RATIO = 0.85f; // 85% del ancho/profundidad total
const float FLOOR_CEILING_MAX_HEIGHT = 0.6f;       // grosor maximo en unidades de mundo

// Indices de malla a excluir manualmente de las colisiones
// (por ejemplo si alguna decoracion muy chica no deberia chocar,
// o si el heuristico de piso/techo falla para algun caso puntual).
// Se llenan mirando el listado [DEBUG] MESH que imprime la consola.
std::vector<int> excludedMeshIndices = { };

// ---------------------------------------------------------
// Cámara
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

struct MeshBoxInfo {
    int index;
    AABB localBox;
    AABB worldBox;
};

AABB g_worldAABB;
std::vector<MeshBoxInfo> g_meshBoxes;   // una caja por cada malla del modelo
std::vector<AABB> collisionWalls;       // subconjunto de g_meshBoxes usado para colisionar

AABB ComputeLocalAABB(const std::vector<Vertex>& vertices)
{
    glm::vec3 vmin(FLT_MAX), vmax(-FLT_MAX);
    for (auto& v : vertices) {
        vmin = glm::min(vmin, v.Position);
        vmax = glm::max(vmax, v.Position);
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

// -----------------------------------------------------------
// Genera una caja de colision por cada malla del modelo y arma
// collisionWalls excluyendo piso/techo (para no bloquear el
// movimiento) y cualquier indice en excludedMeshIndices.
// -----------------------------------------------------------
void BuildMeshCollisionData(Model& model, const glm::mat4& modelMatrix, const AABB& worldBounds)
{
    g_meshBoxes.clear();
    collisionWalls.clear();

    float worldWidth = worldBounds.max.x - worldBounds.min.x;
    float worldDepth = worldBounds.max.z - worldBounds.min.z;

    std::cout << "[DEBUG] Total de mallas en el modelo: " << model.meshes.size() << std::endl;

    for (size_t i = 0; i < model.meshes.size(); i++)
    {
        AABB local = ComputeLocalAABB(model.meshes[i].vertices);
        AABB world = TransformAABB(local, modelMatrix);
        glm::vec3 size = world.max - world.min;

        MeshBoxInfo info{ (int)i, local, world };
        g_meshBoxes.push_back(info);

        bool excludedManually = std::find(excludedMeshIndices.begin(), excludedMeshIndices.end(), (int)i) != excludedMeshIndices.end();

        bool coversFullFootprint = (size.x > worldWidth * FLOOR_CEILING_FOOTPRINT_RATIO) &&
            (size.z > worldDepth * FLOOR_CEILING_FOOTPRINT_RATIO);
        bool isFlat = size.y < FLOOR_CEILING_MAX_HEIGHT;
        bool looksLikeFloorOrCeiling = coversFullFootprint && isFlat;

        std::cout << "  [MESH " << i << "] size=(" << size.x << ", " << size.y << ", " << size.z << ")"
            << (looksLikeFloorOrCeiling ? "  -> tratado como PISO/TECHO (no bloquea)" : "")
            << (excludedManually ? "  -> EXCLUIDO manualmente" : "")
            << std::endl;

        if (excludedManually || looksLikeFloorOrCeiling)
            continue;

        collisionWalls.push_back(world);
    }

    std::cout << "[DEBUG] Mallas usadas como obstaculos solidos: " << collisionWalls.size() << std::endl;
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

    // -------------------- AABB local completo (todas las mallas) --------------------
    glm::vec3 vmin(FLT_MAX), vmax(-FLT_MAX);
    for (auto& mesh : ourModel.meshes)
        for (auto& v : mesh.vertices) {
            vmin = glm::min(vmin, v.Position);
            vmax = glm::max(vmax, v.Position);
        }
    AABB localBox{ vmin, vmax };
    glm::vec3 localSize = localBox.max - localBox.min;

    std::cout << "[DEBUG] Tamano local del modelo completo (sin escalar): "
        << localSize.x << " x " << localSize.y << " x " << localSize.z << std::endl;

    // -------------------- Escala automática --------------------
    float horizontalExtent = std::max(localSize.x, localSize.z);
    float autoScale = (horizontalExtent > 0.0001f) ? (TARGET_BUILDING_SIZE / horizontalExtent) : 1.0f;
    std::cout << "[DEBUG] Escala automatica calculada: " << autoScale << std::endl;

    glm::mat4 warehouseModelMatrix = glm::mat4(1.0f);
    if (MODEL_IS_Z_UP)
        warehouseModelMatrix = glm::rotate(warehouseModelMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    warehouseModelMatrix = glm::translate(warehouseModelMatrix, glm::vec3(0.0f, -localBox.min.y * autoScale, 0.0f));
    warehouseModelMatrix = glm::scale(warehouseModelMatrix, glm::vec3(autoScale));

    // -------------------- AABB de mundo (edificio completo) --------------------
    g_worldAABB = TransformAABB(localBox, warehouseModelMatrix);
    std::cout << "[DEBUG] AABB de mundo: min("
        << g_worldAABB.min.x << ", " << g_worldAABB.min.y << ", " << g_worldAABB.min.z
        << ") max(" << g_worldAABB.max.x << ", " << g_worldAABB.max.y << ", " << g_worldAABB.max.z
        << ")" << std::endl;

    // -------------------- Colisiones por malla (paredes, puertas, sillas, etc.) --------------------
    BuildMeshCollisionData(ourModel, warehouseModelMatrix, g_worldAABB);

    // -------------------- Spawn de la cámara --------------------
    glm::vec3 center = (g_worldAABB.min + g_worldAABB.max) * 0.5f;
    camera.Position = glm::vec3(center.x, g_worldAABB.min.y + PLAYER_EYE_HEIGHT, center.z);
    camera.MovementSpeed = PLAYER_WALK_SPEED;

    // Si el spawn cae dentro de un obstaculo (columna, mesa, etc.),
    // lo empujamos hacia el punto libre mas cercano en X (busqueda simple).
    {
        bool insideSolid = true;
        float step = 0.5f;
        int tries = 0;
        while (insideSolid && tries < 200)
        {
            insideSolid = false;
            for (auto& w : collisionWalls) {
                if (CollidesWithAABB(camera.Position, w, PLAYER_RADIUS)) { insideSolid = true; break; }
            }
            if (insideSolid) {
                camera.Position.x += step;
                tries++;
            }
        }
    }

    std::cout << "[DEBUG] Camara inicial en: ("
        << camera.Position.x << ", " << camera.Position.y << ", " << camera.Position.z
        << ")" << std::endl;

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

    static bool f1WasPressed = false;
    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS)
    {
        if (!f1WasPressed)
            std::cout << "[DEBUG] camera.Position = ("
            << camera.Position.x << ", " << camera.Position.y << ", " << camera.Position.z
            << ")" << std::endl;
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