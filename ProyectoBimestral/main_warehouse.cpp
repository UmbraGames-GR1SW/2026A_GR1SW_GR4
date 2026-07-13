// ============================================================
// main.cpp - Warehouse con colision por GRILLA 2D basada en
// triangulos reales (no bounding boxes por objeto). Esto respeta
// huecos/aberturas del modelo y evita "pegarse" a las paredes.
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
#include <cmath>

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
const float TARGET_BUILDING_SIZE = 40.0f;
const float PLAYER_EYE_HEIGHT = 1.7f;
const float PLAYER_WALK_SPEED = 3.0f;
const float PLAYER_RADIUS = 0.3f;
const bool  MODEL_IS_Z_UP = false;

// -----------------------------------------------------------
// Franja de altura que SI cuenta para colision (relativa al piso).
// Todo triangulo cuyo rango de altura no toque esta franja se
// ignora: asi el piso (muy abajo) y el techo/lamparas (muy arriba)
// nunca bloquean el movimiento, sin necesidad de detectarlos "a mano".
// -----------------------------------------------------------
// Subido de 0.15 a 0.35: los cables tirados en el piso suelen tener
// curvas/ondulaciones que sobresalen unos centimetros, asi que un
// umbral bajo no alcanza. Con 0.35 cualquier TRIANGULO (no malla
// completa) que quede entero por debajo de esa altura se ignora,
// sin importar si comparte malla con una silla u otro objeto solido.
const float COLLISION_BAND_MIN = 0.35f; // por encima del piso
const float COLLISION_BAND_MAX = 2.2f;  // hasta un poco mas alto que la cabeza

// -----------------------------------------------------------
// Respaldo adicional a nivel de malla completa: si TODA una malla
// (cable, hoja, libreta) es chata y esta pegada al piso, se excluye
// entera aunque alguno de sus triangulos rozara la franja de arriba.
// -----------------------------------------------------------
const float FLOOR_CLUTTER_MAX_HEIGHT = 0.3f;
const float FLOOR_CLUTTER_MAX_ELEVATION = 0.4f;

// Si es true, imprime en consola el indice, tamano y posicion de
// cada malla al iniciar. Sirve para identificar cables colgantes
// u otros objetos que quieras excluir a mano en excludedMeshIndices.
const bool PRINT_MESH_DEBUG = true;

// Tamano de celda de la grilla de colision (mas chico = mas preciso, mas memoria)
const float GRID_CELL_SIZE = 0.4f;

// -----------------------------------------------------------
// Mascara de "hay piso aca". Un triangulo cuenta como piso si su
// punto mas bajo esta cerca del nivel del piso general (admite
// pequenas rampas/escalones). Se usa para impedir salir del molde
// real del piso, en vez de depender de un margen artificial.
// -----------------------------------------------------------
const float FLOOR_MASK_MAX_ELEVATION = 0.5f;

// Margen alrededor del edificio para poder salir por aberturas/puertas
// y caminar un poco por el exterior sin chocar con un limite artificial.
const float GRID_SAFETY_MARGIN = 8.0f;

// -----------------------------------------------------------
// Iluminacion de terror: linterna SIEMPRE encendida (con parpadeo de
// bateria vieja) + un glow rojo de fondo siempre presente y tenue +
// niebla que come la visibilidad lejana + vineta en los bordes.
// Todo combinado a la vez, no es un modo que se elige.
// -----------------------------------------------------------
const float FLASHLIGHT_INNER_CUTOFF_DEG = 12.5f;
const float FLASHLIGHT_OUTER_CUTOFF_DEG = 20.0f;

// Parpadeo de linterna: cada FLICKER_STEP_DURATION segundos se decide un
// nuevo nivel de intensidad. La mayoria de las veces es sutil (0.75-1.0);
// ocasionalmente hay un "corte" fuerte simulando mal contacto de bateria.
const float FLICKER_STEP_DURATION = 0.18f;
const float FLICKER_DIP_CHANCE = 0.10f;   // probabilidad de corte fuerte por paso
const float FLICKER_DIP_MIN = 0.08f;      // que tan oscuro llega a quedar en un corte

const glm::vec3 RED_LIGHT_COLOR = glm::vec3(1.0f, 0.05f, 0.05f);
const float RED_LIGHT_BASE_INTENSITY = 0.9f;  // intensidad de cada foco individual (ya no es un glow global)
const float RED_LIGHT_PULSE_AMOUNT = 0.5f;
const float RED_LIGHT_PULSE_SPEED = 1.6f;      // velocidad del "latido"

// Niebla: cuanto mas alto FOG_DENSITY, antes se pierde la visibilidad.
// Subida un poco a pedido: ahora se pierde visibilidad algo antes.
const glm::vec3 FOG_COLOR = glm::vec3(0.03f, 0.01f, 0.01f); // negro con tinte rojizo
const float FOG_DENSITY = 0.006f;

// -----------------------------------------------------------
// Focos de techo: la luz roja ahora sale de puntos reales del modelo
// (los focos/lamparas colgadas del techo) en vez de ser un glow parejo.
// Se detectan automaticamente: mallas chicas ubicadas cerca del techo.
// Si la deteccion automatica falla, llenar manualLightMeshIndices con
// los indices reales (mirando el listado [MESH i] de la consola) y
// esos van a tener prioridad sobre la deteccion automatica.
// -----------------------------------------------------------
const float CEILING_LIGHT_MAX_FOOTPRINT = 3.0f;      // ancho/profundidad maxima para contar como "foco"
const float CEILING_LIGHT_MIN_ELEVATION_FRAC = 0.75f; // debe estar en el 25% superior del edificio
const int   MAX_RED_LIGHTS = 8;
std::vector<int> manualLightMeshIndices = { };

// Indices de malla a excluir por completo de la colision (por ejemplo
// una lampara colgante, cables, o el "shell" exterior/techo si genera
// falsos positivos). Se llenan mirando la consola si algo bloquea mal.
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

// Pseudo-random determinista basado en un valor (para el parpadeo de la linterna)
float PseudoRandom01(float seed)
{
    return glm::fract(sinf(seed * 12.9898f) * 43758.5453f);
}

// ---------------------------------------------------------
// AABB simple (solo se usa para medir el modelo, ya no para colisionar)
// ---------------------------------------------------------
struct AABB {
    glm::vec3 min;
    glm::vec3 max;
};

// ---------------------------------------------------------
// Grilla de colision 2D (XZ). Cada celda es "solida" o "libre".
// ---------------------------------------------------------
struct CollisionGrid {
    float originX = 0.0f, originZ = 0.0f;
    float cellSize = GRID_CELL_SIZE;
    int width = 0, depth = 0;
    std::vector<char> solid;

    inline int ToIndex(int ix, int iz) const { return iz * width + ix; }
    inline bool InBounds(int ix, int iz) const { return ix >= 0 && ix < width && iz >= 0 && iz < depth; }

    void MarkSolidCell(int ix, int iz)
    {
        if (InBounds(ix, iz)) solid[ToIndex(ix, iz)] = 1;
    }

    void MarkSolidWorldBox(float xmin, float xmax, float zmin, float zmax)
    {
        int ix0 = (int)std::floor((xmin - originX) / cellSize);
        int ix1 = (int)std::floor((xmax - originX) / cellSize);
        int iz0 = (int)std::floor((zmin - originZ) / cellSize);
        int iz1 = (int)std::floor((zmax - originZ) / cellSize);
        for (int iz = iz0; iz <= iz1; iz++)
            for (int ix = ix0; ix <= ix1; ix++)
                MarkSolidCell(ix, iz);
    }

    bool IsSolidWorldPos(float x, float z) const
    {
        int ix = (int)std::floor((x - originX) / cellSize);
        int iz = (int)std::floor((z - originZ) / cellSize);
        if (!InBounds(ix, iz)) return false; // fuera de la grilla = zona abierta, no bloquea
        return solid[ToIndex(ix, iz)] != 0;
    }
};

CollisionGrid g_grid;      // 1 = bloqueado (pared, columna, silla, etc.)
CollisionGrid g_floorMask; // 1 = hay piso real debajo

std::vector<glm::vec3> g_meshCenters;         // centro (mundo) de cada malla, indexado igual que model.meshes
std::vector<int> g_autoDetectedLightIndices;  // indices de mallas candidatas a "foco de techo"
std::vector<glm::vec3> g_redLightPositions;   // posiciones finales usadas por el shader

// -----------------------------------------------------------
// Construye la grilla de colision recorriendo TODOS los triangulos
// del modelo. Un triangulo solo aporta colision si su rango de
// altura Y cae dentro de la franja jugador (COLLISION_BAND_*),
// lo que excluye piso y techo automaticamente sin heuristicas
// basadas en "tamano total de la malla".
// -----------------------------------------------------------
void BuildCollisionGrid(Model& model, const glm::mat4& modelMatrix, const AABB& worldBounds)
{
    g_grid.cellSize = GRID_CELL_SIZE;
    g_grid.originX = worldBounds.min.x - GRID_SAFETY_MARGIN;
    g_grid.originZ = worldBounds.min.z - GRID_SAFETY_MARGIN;

    float totalWidth = (worldBounds.max.x - worldBounds.min.x) + 2.0f * GRID_SAFETY_MARGIN;
    float totalDepth = (worldBounds.max.z - worldBounds.min.z) + 2.0f * GRID_SAFETY_MARGIN;

    g_grid.width = (int)std::ceil(totalWidth / g_grid.cellSize) + 1;
    g_grid.depth = (int)std::ceil(totalDepth / g_grid.cellSize) + 1;
    g_grid.solid.assign((size_t)g_grid.width * (size_t)g_grid.depth, 0);

    g_floorMask.cellSize = g_grid.cellSize;
    g_floorMask.originX = g_grid.originX;
    g_floorMask.originZ = g_grid.originZ;
    g_floorMask.width = g_grid.width;
    g_floorMask.depth = g_grid.depth;
    g_floorMask.solid.assign((size_t)g_floorMask.width * (size_t)g_floorMask.depth, 0);

    float floorY = worldBounds.min.y;
    float bandMin = floorY + COLLISION_BAND_MIN;
    float bandMax = floorY + COLLISION_BAND_MAX;

    long long trianglesUsed = 0;
    long long trianglesTotal = 0;

    for (size_t m = 0; m < model.meshes.size(); m++)
    {
        auto& verts = model.meshes[m].vertices;
        auto& idx = model.meshes[m].indices;

        // AABB de la malla completa en espacio de mundo (para decidir si es
        // "basura de piso" y para el print de debug).
        glm::vec3 meshMin(FLT_MAX), meshMax(-FLT_MAX);
        for (auto& v : verts) {
            glm::vec3 wp = glm::vec3(modelMatrix * glm::vec4(v.Position, 1.0f));
            meshMin = glm::min(meshMin, wp);
            meshMax = glm::max(meshMax, wp);
        }
        float meshHeight = meshMax.y - meshMin.y;
        float elevationAboveFloor = meshMin.y - floorY;

        bool excludedManually = std::find(excludedMeshIndices.begin(), excludedMeshIndices.end(), (int)m) != excludedMeshIndices.end();
        bool isFloorClutter = (meshHeight < FLOOR_CLUTTER_MAX_HEIGHT) &&
            (elevationAboveFloor > -0.05f) &&
            (elevationAboveFloor < FLOOR_CLUTTER_MAX_ELEVATION);

        // Centro de la malla en mundo (se guarda siempre, sirve tanto para
        // deteccion automatica de focos como para overrides manuales).
        glm::vec3 meshCenter = (meshMin + meshMax) * 0.5f;
        g_meshCenters.push_back(meshCenter);

        // Candidato a "foco de techo": chico y ubicado en el 25% superior
        // del edificio. No se excluye de colision/piso por esto, solo se
        // usa para decidir de donde sale la luz roja.
        float elevationFrac = (worldBounds.max.y > worldBounds.min.y)
            ? (meshCenter.y - floorY) / (worldBounds.max.y - floorY)
            : 0.0f;
        float footprint = std::max(meshMax.x - meshMin.x, meshMax.z - meshMin.z);
        bool isCeilingLightCandidate = !excludedManually &&
            footprint < CEILING_LIGHT_MAX_FOOTPRINT &&
            elevationFrac > CEILING_LIGHT_MIN_ELEVATION_FRAC;
        if (isCeilingLightCandidate)
            g_autoDetectedLightIndices.push_back((int)m);

        if (PRINT_MESH_DEBUG)
        {
            std::cout << "  [MESH " << m << "] verts=" << verts.size()
                << " size=(" << (meshMax.x - meshMin.x) << ", " << meshHeight << ", " << (meshMax.z - meshMin.z) << ")"
                << " elevacion=" << elevationAboveFloor
                << (isFloorClutter ? "  -> BASURA DE PISO (excluida auto)" : "")
                << (isCeilingLightCandidate ? "  -> CANDIDATO A FOCO DE TECHO" : "")
                << (excludedManually ? "  -> EXCLUIDA manualmente" : "")
                << std::endl;
        }

        // La exclusion MANUAL (ej. cables colgantes) descarta el objeto
        // por completo, tambien de la mascara de piso (tiene sentido:
        // un cable colgado no es piso). La exclusion por "basura de
        // piso" en cambio NO debe saltear la mascara de piso: si hay un
        // cable/hoja tirado sobre el piso real, el piso real sigue
        // estando ahi debajo.
        if (excludedManually)
            continue;

        for (size_t i = 0; i + 2 < idx.size(); i += 3)
        {
            trianglesTotal++;

            glm::vec3 p0 = glm::vec3(modelMatrix * glm::vec4(verts[idx[i]].Position, 1.0f));
            glm::vec3 p1 = glm::vec3(modelMatrix * glm::vec4(verts[idx[i + 1]].Position, 1.0f));
            glm::vec3 p2 = glm::vec3(modelMatrix * glm::vec4(verts[idx[i + 2]].Position, 1.0f));

            float yMin = std::min({ p0.y, p1.y, p2.y });
            float yMax = std::max({ p0.y, p1.y, p2.y });

            float xMin = std::min({ p0.x, p1.x, p2.x });
            float xMax = std::max({ p0.x, p1.x, p2.x });
            float zMin = std::min({ p0.z, p1.z, p2.z });
            float zMax = std::max({ p0.z, p1.z, p2.z });

            // Mascara de piso: el triangulo cuenta como "hay piso aca" si
            // su punto mas bajo esta cerca del nivel general del piso.
            // Esto es independiente de si bloquea el paso o no.
            if (yMin - floorY < FLOOR_MASK_MAX_ELEVATION)
                g_floorMask.MarkSolidWorldBox(xMin, xMax, zMin, zMax);

            // Colision: se salta si la malla entera es "basura de piso"
            // o si el triangulo no toca la franja del jugador (esto
            // descarta piso y techo automaticamente).
            if (isFloorClutter)
                continue;
            if (yMax < bandMin || yMin > bandMax)
                continue;

            g_grid.MarkSolidWorldBox(xMin, xMax, zMin, zMax);
            trianglesUsed++;
        }
    }

    long long solidCells = 0;
    for (char c : g_grid.solid) if (c) solidCells++;

    long long floorCells = 0;
    for (char c : g_floorMask.solid) if (c) floorCells++;

    std::cout << "[DEBUG] Grilla: " << g_grid.width << " x " << g_grid.depth
        << " celdas (" << g_grid.cellSize << " unidades c/u)" << std::endl;
    std::cout << "[DEBUG] Triangulos totales: " << trianglesTotal
        << " | usados para colision: " << trianglesUsed << std::endl;
    std::cout << "[DEBUG] Celdas solidas: " << solidCells << " / " << g_grid.solid.size() << std::endl;
    std::cout << "[DEBUG] Celdas con piso: " << floorCells << " / " << g_floorMask.solid.size() << std::endl;

    // -------------------- Posiciones finales de las luces rojas --------------------
    g_redLightPositions.clear();
    if (!manualLightMeshIndices.empty())
    {
        for (int idx : manualLightMeshIndices)
        {
            if (idx >= 0 && idx < (int)g_meshCenters.size())
                g_redLightPositions.push_back(g_meshCenters[idx]);
        }
        std::cout << "[DEBUG] Usando " << g_redLightPositions.size() << " foco(s) definidos manualmente en manualLightMeshIndices." << std::endl;
    }
    else
    {
        for (int idx : g_autoDetectedLightIndices)
        {
            if ((int)g_redLightPositions.size() >= MAX_RED_LIGHTS) break;
            g_redLightPositions.push_back(g_meshCenters[idx]);
        }
        std::cout << "[DEBUG] " << g_autoDetectedLightIndices.size() << " candidato(s) a foco de techo detectados automaticamente, usando "
            << g_redLightPositions.size() << "." << std::endl;
    }

    // Respaldo: si no se detecto ni configuro ningun foco, usamos un
    // punto por defecto en el centro del techo para no quedarnos sin luz roja.
    if (g_redLightPositions.empty())
    {
        glm::vec3 fallback((worldBounds.min.x + worldBounds.max.x) * 0.5f,
            worldBounds.max.y - 0.3f,
            (worldBounds.min.z + worldBounds.max.z) * 0.5f);
        g_redLightPositions.push_back(fallback);
        std::cout << "[DEBUG] No se detecto ningun foco de techo: usando 1 punto de respaldo en el centro del techo." << std::endl;
    }

    for (size_t i = 0; i < g_redLightPositions.size(); i++)
        std::cout << "  [LUZ ROJA " << i << "] pos=(" << g_redLightPositions[i].x << ", "
        << g_redLightPositions[i].y << ", " << g_redLightPositions[i].z << ")" << std::endl;
}

// Hay piso pisable en esta posicion? (usa el centro del jugador, no el
// radio completo, para no bloquear el borde del piso antes de tiempo).
bool HasFloorAt(const glm::vec3& pos)
{
    return g_floorMask.IsSolidWorldPos(pos.x, pos.z);
}

// Revisa si el cuerpo del jugador (circulo de radio "radius")
// pisa alguna celda solida en la posicion dada. Por defecto usa
// PLAYER_RADIUS, pero admite un radio mayor para busquedas de
// spawn que necesiten margen extra de seguridad.
bool CollidesAt(const glm::vec3& pos, float radius = PLAYER_RADIUS)
{
    if (g_grid.IsSolidWorldPos(pos.x, pos.z)) return true;

    const int SAMPLES = 8;
    for (int i = 0; i < SAMPLES; i++)
    {
        float angle = (2.0f * 3.14159265f * i) / SAMPLES;
        float sx = pos.x + cosf(angle) * radius;
        float sz = pos.z + sinf(angle) * radius;
        if (g_grid.IsSolidWorldPos(sx, sz)) return true;
    }
    return false;
}

// Movimiento con "sliding" por eje, usando la grilla en vez de AABBs.
// Un movimiento es valido si no choca con nada Y si hay piso real ahi
// (evita salir del molde del piso del modelo).
glm::vec3 TryMove(const glm::vec3& currentPos, const glm::vec3& delta)
{
    glm::vec3 newPos = currentPos;

    glm::vec3 testX = newPos + glm::vec3(delta.x, 0.0f, 0.0f);
    if (!CollidesAt(testX) && HasFloorAt(testX)) newPos.x = testX.x;

    glm::vec3 testZ = newPos + glm::vec3(0.0f, 0.0f, delta.z);
    if (!CollidesAt(testZ) && HasFloorAt(testZ)) newPos.z = testZ.z;

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

    // -------------------- AABB local completo (para medir/escalar) --------------------
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

    // -------------------- AABB de mundo (edificio completo, solo para medir) --------------------
    glm::vec3 wmin(FLT_MAX), wmax(-FLT_MAX);
    {
        glm::vec3 corners[8] = {
            {localBox.min.x, localBox.min.y, localBox.min.z}, {localBox.max.x, localBox.min.y, localBox.min.z},
            {localBox.min.x, localBox.max.y, localBox.min.z}, {localBox.min.x, localBox.min.y, localBox.max.z},
            {localBox.max.x, localBox.max.y, localBox.min.z}, {localBox.max.x, localBox.min.y, localBox.max.z},
            {localBox.min.x, localBox.max.y, localBox.max.z}, {localBox.max.x, localBox.max.y, localBox.max.z},
        };
        for (auto& c : corners) {
            glm::vec3 wc = glm::vec3(warehouseModelMatrix * glm::vec4(c, 1.0f));
            wmin = glm::min(wmin, wc);
            wmax = glm::max(wmax, wc);
        }
    }
    AABB g_worldAABB{ wmin, wmax };
    std::cout << "[DEBUG] AABB de mundo: min("
        << g_worldAABB.min.x << ", " << g_worldAABB.min.y << ", " << g_worldAABB.min.z
        << ") max(" << g_worldAABB.max.x << ", " << g_worldAABB.max.y << ", " << g_worldAABB.max.z
        << ")" << std::endl;

    // -------------------- Grilla de colision (paredes, columnas, sillas, etc.) --------------------
    BuildCollisionGrid(ourModel, warehouseModelMatrix, g_worldAABB);

    // -------------------- Spawn de la cámara --------------------
    glm::vec3 center = (g_worldAABB.min + g_worldAABB.max) * 0.5f;
    camera.Position = glm::vec3(center.x, g_worldAABB.min.y + PLAYER_EYE_HEIGHT, center.z);
    camera.MovementSpeed = PLAYER_WALK_SPEED;

    // Buscamos un punto de spawn con margen extra (no solo "apenas libre"),
    // para no aparecer pegado a una pared/columna y sentir la camara trabada
    // desde el primer instante.
    const float SPAWN_CLEARANCE = PLAYER_RADIUS + 0.2f;
    if (CollidesAt(camera.Position, SPAWN_CLEARANCE) || !HasFloorAt(camera.Position))
    {
        bool found = false;
        for (int radius = 1; radius <= 60 && !found; radius++)
        {
            float r = radius * (GRID_CELL_SIZE * 0.5f);
            for (int a = 0; a < 16 && !found; a++)
            {
                float angle = (2.0f * 3.14159265f * a) / 16;
                glm::vec3 candidate = camera.Position;
                candidate.x = center.x + cosf(angle) * r;
                candidate.z = center.z + sinf(angle) * r;
                if (!CollidesAt(candidate, SPAWN_CLEARANCE) && HasFloorAt(candidate))
                {
                    camera.Position.x = candidate.x;
                    camera.Position.z = candidate.z;
                    found = true;
                }
            }
        }
    }

    std::cout << "[DEBUG] Camara inicial en: ("
        << camera.Position.x << ", " << camera.Position.y << ", " << camera.Position.z
        << ")" << std::endl;

    // Reseteamos el reloj aca (no antes): cargar el modelo puede tardar
    // varios segundos, y si no hacemos esto el primer deltaTime del loop
    // seria enorme, generando un salto de movimiento brusco que te deja
    // "pegado" contra lo primero que encuentre.
    lastFrame = static_cast<float>(glfwGetTime());

    // -------------------- Render loop --------------------
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Limite de seguridad: si hay un frame lento (carga de textura,
        // hitch del sistema, etc.), no dejamos que un deltaTime gigante
        // mande al jugador de un salto contra una pared.
        if (deltaTime > 0.1f) deltaTime = 0.1f;

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

        // -------- Iluminacion de terror (siempre combinada) --------
        ourShader.setVec3("viewPos", camera.Position);
        ourShader.setVec3("flashlightDir", camera.Front);
        ourShader.setFloat("flashlightCutOff", cosf(glm::radians(FLASHLIGHT_INNER_CUTOFF_DEG)));
        ourShader.setFloat("flashlightOuterCutOff", cosf(glm::radians(FLASHLIGHT_OUTER_CUTOFF_DEG)));

        // Parpadeo de bateria: se recalcula cada FLICKER_STEP_DURATION segundos,
        // no cada frame, para que se sienta como pasos de parpadeo y no un ruido continuo.
        float flickerStep = floorf(currentFrame / FLICKER_STEP_DURATION);
        float flickerRoll = PseudoRandom01(flickerStep);
        float flashlightFlicker;
        if (flickerRoll < FLICKER_DIP_CHANCE)
        {
            float dipAmount = PseudoRandom01(flickerStep * 7.31f);
            flashlightFlicker = FLICKER_DIP_MIN + dipAmount * 0.15f;
        }
        else
        {
            flashlightFlicker = 0.8f + PseudoRandom01(flickerStep * 3.17f) * 0.2f;
        }
        ourShader.setFloat("flashlightFlicker", flashlightFlicker);

        // Palpitar tipo latido para las luces rojas del techo
        float pulse = RED_LIGHT_BASE_INTENSITY + RED_LIGHT_PULSE_AMOUNT *
            (0.5f + 0.5f * sinf(currentFrame * RED_LIGHT_PULSE_SPEED));
        ourShader.setVec3("redLightColor", RED_LIGHT_COLOR);
        ourShader.setFloat("redLightIntensity", pulse);
        ourShader.setInt("numRedLights", (int)g_redLightPositions.size());
        for (size_t i = 0; i < g_redLightPositions.size(); i++)
            ourShader.setVec3("redLightPositions[" + std::to_string(i) + "]", g_redLightPositions[i]);

        // Niebla y vineta
        ourShader.setVec3("fogColor", FOG_COLOR);
        ourShader.setFloat("fogDensity", FOG_DENSITY);
        ourShader.setVec2("screenSize", glm::vec2((float)SCR_WIDTH, (float)SCR_HEIGHT));

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

    camera.Position = TryMove(camera.Position, delta);
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