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
#include <string>
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

// -----------------------------------------------------------
// Modelo extra: se carga, escala y ubica en el centro del warehouse, a
// nivel de piso, y se dibuja con el MISMO shader que el resto (hereda
// linterna, luces rojas, niebla, grano, etc. automaticamente).
// -----------------------------------------------------------
const bool ADD_EXTRA_MODEL = true;
const std::string EXTRA_MODEL_PATH = "C:/Users/redin/source/repos/2026A_GR1SW_GR4/ProyectoBimestral/model/C_Tree/C_tree.obj";
const float EXTRA_MODEL_TARGET_HEIGHT = 4.0f; // altura deseada en unidades de mundo, ajustable
const bool  EXTRA_MODEL_IS_Z_UP = false;       // poner true si aparece "acostado"
const bool  EXTRA_MODEL_ADD_COLLISION = true;  // false si no queres que bloquee el paso

// ---------------------------------------------------------
// Configuración de escala / jugador
// ---------------------------------------------------------
const float TARGET_BUILDING_SIZE = 40.0f;
const float PLAYER_EYE_HEIGHT = 1.7f;

// -----------------------------------------------------------
// Esquina de spawn: 0.0 = pegado a la pared del minimo de ese eje,
// 1.0 = pegado a la pared del maximo, 0.5 = centro. Para spawnear en
// la esquina OPUESTA a la puerta, probar las 4 combinaciones (0,0),
// (1,0), (0,1), (1,1) y quedarse con la que quede lejos de la puerta
// (no sabemos de este lado del codigo donde esta la puerta exactamente).
// -----------------------------------------------------------
const float SPAWN_X_FRAC = 0.1f;
const float SPAWN_Z_FRAC = 0.1f;
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
const float FLASHLIGHT_INNER_CUTOFF_DEG = 13.0f;
const float FLASHLIGHT_OUTER_CUTOFF_DEG = 21.0f;

// Parpadeo de linterna: cada FLICKER_STEP_DURATION segundos se decide un
// nuevo nivel de intensidad. La mayoria de las veces es sutil (0.75-1.0);
// ocasionalmente hay un "corte" fuerte simulando mal contacto de bateria.
const float FLICKER_STEP_DURATION = 0.18f;
const float FLICKER_DIP_CHANCE = 0.10f;   // probabilidad de corte fuerte por paso
const float FLICKER_DIP_MIN = 0.08f;      // que tan oscuro llega a quedar en un corte

// -----------------------------------------------------------
// Apagon real: cada tanto la linterna se va a negro por completo
// durante un ratito, distinto del parpadeo rapido de arriba (que es
// un temblor sutil constante). Esto es "la bateria fallo en serio".
// El proximo apagon se sortea dentro de [MIN_INTERVAL, MAX_INTERVAL]
// segundos despues del anterior, y dura entre [MIN_DURATION, MAX_DURATION].
// -----------------------------------------------------------
const float BLACKOUT_MIN_INTERVAL = 12.0f;
const float BLACKOUT_MAX_INTERVAL = 28.0f;
const float BLACKOUT_MIN_DURATION = 1.0f;
const float BLACKOUT_MAX_DURATION = 3.0f;

const glm::vec3 RED_LIGHT_COLOR = glm::vec3(1.0f, 0.05f, 0.05f);
const float RED_LIGHT_BASE_INTENSITY = 1.3f;   // subido: ahora se nota bien incluso lejos
const float RED_LIGHT_PULSE_AMOUNT = 0.9f;     // rango grande de sube-y-baja
const float RED_LIGHT_PULSE_SPEED = 0.55f;     // mas lento: ciclo completo dura ~11s, se percibe como "respirar"

// Niebla: cuanto mas alto FOG_DENSITY, antes se pierde la visibilidad.
// Mas oscura y densa que antes: el fondo se pierde en negro casi total.
const glm::vec3 FOG_COLOR = glm::vec3(0.012f, 0.003f, 0.003f);
const float FOG_DENSITY = 0.012f;

// -----------------------------------------------------------
// Focos de techo: la luz roja sale de puntos sobre el techo real del
// edificio. Hay 3 formas de definir esos puntos, en este orden de
// prioridad:
//   1) manualLightMeshIndices (si lo llenas, siempre gana)
//   2) deteccion automatica por geometria (USE_AUTO_DETECTED_CEILING_LIGHTS)
//   3) GRILLA PAREJA por todo el techo (por defecto, mas confiable)
//
// La deteccion automatica probo confundir lamparas con dinteles/marcos
// de puerta en este modelo, asi que por defecto queda DESACTIVADA y se
// usa la grilla: reparte NxM puntos por TODO el techo del edificio (con
// margen hacia adentro de las paredes), lo que garantiza luz en ambos
// lados del edificio sin depender de adivinar que malla es una lampara.
//
// Ademas se dibuja una "bombilla" visible (billboard con glow) en cada
// posicion, asi podes VER en el juego donde esta cada foco.
// -----------------------------------------------------------
const bool  USE_AUTO_DETECTED_CEILING_LIGHTS = false; // recomendado: false (usar grilla)

const float CEILING_LIGHT_MAX_FOOTPRINT = 1.5f;
const float CEILING_LIGHT_MIN_ELEVATION_FRAC = 0.90f;
const float CEILING_LIGHT_MIN_WALL_MARGIN = 1.5f;
const float CEILING_LIGHT_MAX_ASPECT_RATIO = 2.0f;

const int   CEILING_LIGHT_GRID_COLS = 3;
const int   CEILING_LIGHT_GRID_ROWS = 3;
const float CEILING_LIGHT_GRID_MARGIN_FRAC = 0.15f; // que tan adentro de las paredes arranca la grilla

const int   MAX_RED_LIGHTS = 9; // 3x3
std::vector<int> manualLightMeshIndices = { };

// Tamano visual de la "bombilla" que se dibuja en cada foco (unidades de mundo)
const float LIGHT_BULB_SCALE = 0.6f;

// Gradacion de color: la parte "neutra" de la escena (ambiente + linterna)
// se desatura hacia gris, pero el rojo de las lamparas queda intacto y
// bien saturado -- asi el rojo resalta mucho mas contra todo lo demas.
const float DESATURATION_AMOUNT = 0.55f;

// Brillo especular: mas alto = destellos mas chicos y nitidos (superficie
// muy lisa/metalica/mojada); mas bajo = brillo mas difuso y amplio.
const float MATERIAL_SHININESS = 24.0f;

// Contraste crudo: >1 aplasta las sombras a negro (no se "adivina" lo que
// hay en la oscuridad); 1.0 = sin cambio. Grano: textura sucia tipo
// camara vieja/found footage, 0 = sin grano.
const float CONTRAST_POWER = 1.35f;
const float GRAIN_AMOUNT = 0.065f;

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

// Estado del apagon real de la linterna
bool g_inBlackout = false;
float g_nextBlackoutTime = -1.0f; // se inicializa la primera vez que se usa
float g_blackoutEndTime = 0.0f;

// Golpe visual: momento en que se disparo el ultimo flash (para el corte
// de linterna). -1000 = nunca, asi el primer frame no dispara nada.
float g_startleTriggerTime = -1000.0f;
const float STARTLE_FLASH_DURATION = 0.18f;

// Apagon real por foco: cada luz roja tiene su propio ciclo de
// encendido/apagado (periodo y duracion del apagon distintos entre si,
// para que no se apaguen todas juntas). Es deterministico en base al
// tiempo, no necesita guardar estado -- salvo un vector para detectar
// la transicion y loguearla en consola.
const float RED_LIGHT_OFF_PERIOD_MIN = 28.0f;   // cada cuanto (min) le toca un apagon a una luz
const float RED_LIGHT_OFF_PERIOD_MAX = 45.0f;   // cada cuanto (max)
const float RED_LIGHT_OFF_DURATION_MIN = 12.0f; // cuanto dura el apagon (min) -- largo, para que se note bien
const float RED_LIGHT_OFF_DURATION_MAX = 18.0f; // cuanto dura el apagon (max)
std::vector<bool> g_redLightWasOff;

// Pseudo-random determinista basado en un valor (para el parpadeo de la linterna)
float PseudoRandom01(float seed)
{
    return glm::fract(sinf(seed * 12.9898f) * 43758.5453f);
}

// Pseudo-random en un rango [minVal, maxVal], variando la semilla para no
// repetir siempre el mismo numero cuando se llama varias veces seguidas.
float PseudoRandomRange(float seed, float minVal, float maxVal)
{
    return minVal + PseudoRandom01(seed) * (maxVal - minVal);
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
        float footprintX = meshMax.x - meshMin.x;
        float footprintZ = meshMax.z - meshMin.z;
        float footprint = std::max(footprintX, footprintZ);

        // Distancia horizontal a la pared exterior mas cercana (usa el AABB
        // completo del edificio como aproximacion del perimetro). Un dintel
        // de puerta esta pegado a la pared -> distancia chica. Una lampara
        // que cuelga del techo suele estar mas hacia adentro.
        float distToWallX = std::min(meshCenter.x - worldBounds.min.x, worldBounds.max.x - meshCenter.x);
        float distToWallZ = std::min(meshCenter.z - worldBounds.min.z, worldBounds.max.z - meshCenter.z);
        float distToNearestWall = std::min(distToWallX, distToWallZ);

        // Aspect ratio: un dintel/viga es angosto y largo (aspect ratio alto);
        // una lampara es mas pareja en ambos ejes horizontales.
        float aspectRatio = std::max(footprintX, footprintZ) / std::max(std::min(footprintX, footprintZ), 0.01f);

        bool isCeilingLightCandidate = !excludedManually &&
            footprint < CEILING_LIGHT_MAX_FOOTPRINT &&
            elevationFrac > CEILING_LIGHT_MIN_ELEVATION_FRAC &&
            distToNearestWall > CEILING_LIGHT_MIN_WALL_MARGIN &&
            aspectRatio < CEILING_LIGHT_MAX_ASPECT_RATIO;
        if (isCeilingLightCandidate)
            g_autoDetectedLightIndices.push_back((int)m);

        if (PRINT_MESH_DEBUG)
        {
            std::cout << "  [MESH " << m << "] verts=" << verts.size()
                << " size=(" << footprintX << ", " << meshHeight << ", " << footprintZ << ")"
                << " elevacion=" << elevationAboveFloor
                << " distPared=" << distToNearestWall
                << " aspect=" << aspectRatio
                << (isFloorClutter ? "  -> BASURA DE PISO (excluida auto)" : "")
                << (isCeilingLightCandidate ? "  -> CANDIDATO A FOCO DE TECHO" : "")
                << (excludedManually ? "  -> EXCLUIDA manualmente" : "")
                << "\n"; // "\n" en vez de std::endl: evita forzar un flush por cada malla (mas rapido si hay muchas)
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
    else if (USE_AUTO_DETECTED_CEILING_LIGHTS && !g_autoDetectedLightIndices.empty())
    {
        for (int idx : g_autoDetectedLightIndices)
        {
            if ((int)g_redLightPositions.size() >= MAX_RED_LIGHTS) break;
            g_redLightPositions.push_back(g_meshCenters[idx]);
        }
        std::cout << "[DEBUG] " << g_autoDetectedLightIndices.size() << " candidato(s) a foco de techo detectados automaticamente, usando "
            << g_redLightPositions.size() << "." << std::endl;
    }
    else
    {
        // Grilla pareja por todo el techo real del edificio: garantiza
        // cobertura en TODOS los lados, sin depender de adivinar que
        // malla es una lampara real.
        float marginX = (worldBounds.max.x - worldBounds.min.x) * CEILING_LIGHT_GRID_MARGIN_FRAC;
        float marginZ = (worldBounds.max.z - worldBounds.min.z) * CEILING_LIGHT_GRID_MARGIN_FRAC;
        float xStart = worldBounds.min.x + marginX;
        float xEnd = worldBounds.max.x - marginX;
        float zStart = worldBounds.min.z + marginZ;
        float zEnd = worldBounds.max.z - marginZ;
        float y = worldBounds.max.y - 0.3f;

        for (int r = 0; r < CEILING_LIGHT_GRID_ROWS; r++)
        {
            for (int c = 0; c < CEILING_LIGHT_GRID_COLS; c++)
            {
                if ((int)g_redLightPositions.size() >= MAX_RED_LIGHTS) break;
                float tx = (CEILING_LIGHT_GRID_COLS > 1) ? (float)c / (float)(CEILING_LIGHT_GRID_COLS - 1) : 0.5f;
                float tz = (CEILING_LIGHT_GRID_ROWS > 1) ? (float)r / (float)(CEILING_LIGHT_GRID_ROWS - 1) : 0.5f;
                float x = xStart + tx * (xEnd - xStart);
                float z = zStart + tz * (zEnd - zStart);
                g_redLightPositions.push_back(glm::vec3(x, y, z));
            }
        }
        std::cout << "[DEBUG] Usando grilla pareja de " << g_redLightPositions.size()
            << " focos repartidos por todo el techo (cubre ambos lados del edificio)." << std::endl;
    }

    for (size_t i = 0; i < g_redLightPositions.size(); i++)
        std::cout << "  [LUZ ROJA " << i << "] pos=(" << g_redLightPositions[i].x << ", "
        << g_redLightPositions[i].y << ", " << g_redLightPositions[i].z << ")" << std::endl;
}

// -----------------------------------------------------------
// Suma la colision de un modelo EXTRA (ej. el arbol) a la grilla que ya
// se construyo para el warehouse -- no la reconstruye, solo agrega mas
// celdas solidas. Usa la misma franja de altura que el resto, asi que
// el piso/techo del modelo extra (si tuviera) tambien se ignora igual.
// -----------------------------------------------------------
void AddModelCollision(Model& model, const glm::mat4& modelMatrix, float floorY)
{
    float bandMin = floorY + COLLISION_BAND_MIN;
    float bandMax = floorY + COLLISION_BAND_MAX;
    long long trianglesAdded = 0;

    for (auto& mesh : model.meshes)
    {
        auto& verts = mesh.vertices;
        auto& idx = mesh.indices;
        for (size_t i = 0; i + 2 < idx.size(); i += 3)
        {
            glm::vec3 p0 = glm::vec3(modelMatrix * glm::vec4(verts[idx[i]].Position, 1.0f));
            glm::vec3 p1 = glm::vec3(modelMatrix * glm::vec4(verts[idx[i + 1]].Position, 1.0f));
            glm::vec3 p2 = glm::vec3(modelMatrix * glm::vec4(verts[idx[i + 2]].Position, 1.0f));

            float yMin = std::min({ p0.y, p1.y, p2.y });
            float yMax = std::max({ p0.y, p1.y, p2.y });
            if (yMax < bandMin || yMin > bandMax)
                continue;

            float xMin = std::min({ p0.x, p1.x, p2.x });
            float xMax = std::max({ p0.x, p1.x, p2.x });
            float zMin = std::min({ p0.z, p1.z, p2.z });
            float zMax = std::max({ p0.z, p1.z, p2.z });

            g_grid.MarkSolidWorldBox(xMin, xMax, zMin, zMax);
            trianglesAdded++;
        }
    }
    std::cout << "[DEBUG] Colision agregada para modelo extra: " << trianglesAdded << " triangulos." << std::endl;
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

    // Frame de "cargando" inmediato: sin esto, la ventana queda con
    // contenido indefinido (se ve como colgada) mientras se cargan
    // shaders, texturas y el modelo, que puede tardar varios segundos.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwSwapBuffers(window);
    glfwPollEvents();
    std::cout << "[DEBUG] Cargando modelo (puede tardar varios segundos si tiene muchas texturas/mallas)..." << std::endl;

    // -------------------- Shaders --------------------
    Shader ourShader("shaders/model_loading.vs", "shaders/model_loading.fs");
    Shader bulbShader("shaders/light_billboard.vs", "shaders/light_billboard.fs");

    // -------------------- Quad para las "bombillas" visibles --------------------
    float bulbQuad[] = {
        -0.5f, -0.5f,   0.5f, -0.5f,   0.5f, 0.5f,
        -0.5f, -0.5f,   0.5f,  0.5f,  -0.5f, 0.5f,
    };
    unsigned int bulbVAO, bulbVBO;
    glGenVertexArrays(1, &bulbVAO);
    glGenBuffers(1, &bulbVBO);
    glBindVertexArray(bulbVAO);
    glBindBuffer(GL_ARRAY_BUFFER, bulbVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bulbQuad), bulbQuad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

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

    // -------------------- Modelo extra (arbol), centrado y a nivel de piso --------------------
    Model* extraModel = nullptr;
    glm::mat4 extraModelMatrix = glm::mat4(1.0f);
    if (ADD_EXTRA_MODEL)
    {
        extraModel = new Model(EXTRA_MODEL_PATH);

        glm::vec3 emin(FLT_MAX), emax(-FLT_MAX);
        for (auto& mesh : extraModel->meshes)
            for (auto& v : mesh.vertices) {
                emin = glm::min(emin, v.Position);
                emax = glm::max(emax, v.Position);
            }
        glm::vec3 eSize = emax - emin;
        float eHeight = eSize.y;
        float extraScale = (eHeight > 0.0001f) ? (EXTRA_MODEL_TARGET_HEIGHT / eHeight) : 1.0f;

        glm::vec2 localCenterXZ((emin.x + emax.x) * 0.5f, (emin.z + emax.z) * 0.5f);
        glm::vec2 buildingCenterXZ((g_worldAABB.min.x + g_worldAABB.max.x) * 0.5f, (g_worldAABB.min.z + g_worldAABB.max.z) * 0.5f);
        float floorY = g_worldAABB.min.y;

        glm::vec3 worldPos(
            buildingCenterXZ.x - localCenterXZ.x * extraScale,
            floorY - emin.y * extraScale,
            buildingCenterXZ.y - localCenterXZ.y * extraScale
        );

        extraModelMatrix = glm::translate(glm::mat4(1.0f), worldPos);
        if (EXTRA_MODEL_IS_Z_UP)
            extraModelMatrix = glm::rotate(extraModelMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        extraModelMatrix = glm::scale(extraModelMatrix, glm::vec3(extraScale));

        std::cout << "[DEBUG] Modelo extra cargado. Escala=" << extraScale
            << " (altura local=" << eHeight << " -> objetivo=" << EXTRA_MODEL_TARGET_HEIGHT << ")" << std::endl;

        if (EXTRA_MODEL_ADD_COLLISION)
            AddModelCollision(*extraModel, extraModelMatrix, floorY);
    }

    // -------------------- Spawn de la cámara --------------------
    glm::vec3 spawnTarget(
        g_worldAABB.min.x + SPAWN_X_FRAC * (g_worldAABB.max.x - g_worldAABB.min.x),
        g_worldAABB.min.y + PLAYER_EYE_HEIGHT,
        g_worldAABB.min.z + SPAWN_Z_FRAC * (g_worldAABB.max.z - g_worldAABB.min.z)
    );
    camera.Position = spawnTarget;
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
                candidate.x = spawnTarget.x + cosf(angle) * r;
                candidate.z = spawnTarget.z + sinf(angle) * r;
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

    // -------------------- Orientar la camara hacia el centro del edificio --------------------
    // Por defecto la camara mira hacia -Z, que puede ser directo a una pared
    // segun donde caiga el spawn. La giramos para que apunte hacia el centro
    // del edificio (en el plano horizontal), asi arrancas viendo el espacio
    // en general y no la cara de un muro.
    {
        glm::vec3 buildingCenterXZ(
            (g_worldAABB.min.x + g_worldAABB.max.x) * 0.5f,
            camera.Position.y,
            (g_worldAABB.min.z + g_worldAABB.max.z) * 0.5f
        );
        glm::vec3 toCenter = buildingCenterXZ - camera.Position;
        toCenter.y = 0.0f;
        if (glm::length(toCenter) > 0.001f)
        {
            toCenter = glm::normalize(toCenter);
            camera.Yaw = glm::degrees(atan2f(toCenter.z, toCenter.x));
            camera.Pitch = 0.0f;

            // Recalculamos Front/Right/Up a mano con la misma formula que usa
            // la clase Camera internamente (updateCameraVectors es privado,
            // no lo podemos llamar desde aca) para que quede correcto ya en
            // este mismo frame, sin esperar al primer movimiento de mouse.
            glm::vec3 newFront;
            newFront.x = cosf(glm::radians(camera.Yaw)) * cosf(glm::radians(camera.Pitch));
            newFront.y = sinf(glm::radians(camera.Pitch));
            newFront.z = sinf(glm::radians(camera.Yaw)) * cosf(glm::radians(camera.Pitch));
            camera.Front = glm::normalize(newFront);
            camera.Right = glm::normalize(glm::cross(camera.Front, camera.WorldUp));
            camera.Up = glm::normalize(glm::cross(camera.Right, camera.Front));
        }
    }

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

        // Apagon real: cada tanto la linterna se corta por completo un ratito.
        // Se maneja con un pequeno estado: si no hay apagon programado, se
        // sortea el proximo; si llega la hora, entra en apagon por una
        // duracion tambien sorteada; al terminar, se programa el siguiente.
        if (g_nextBlackoutTime < 0.0f)
            g_nextBlackoutTime = currentFrame + PseudoRandomRange(1.0f, BLACKOUT_MIN_INTERVAL, BLACKOUT_MAX_INTERVAL);

        if (!g_inBlackout && currentFrame >= g_nextBlackoutTime)
        {
            g_inBlackout = true;
            g_blackoutEndTime = currentFrame + PseudoRandomRange(currentFrame, BLACKOUT_MIN_DURATION, BLACKOUT_MAX_DURATION);
            g_startleTriggerTime = currentFrame;
            std::cout << "[DEBUG] Apagon de linterna iniciado." << std::endl;
        }
        else if (g_inBlackout && currentFrame >= g_blackoutEndTime)
        {
            g_inBlackout = false;
            g_nextBlackoutTime = currentFrame + PseudoRandomRange(currentFrame * 1.37f, BLACKOUT_MIN_INTERVAL, BLACKOUT_MAX_INTERVAL);
            std::cout << "[DEBUG] Linterna volvio." << std::endl;
        }

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

        // El apagon real pisa al parpadeo sutil: mientras dura, la linterna
        // no se recupera aunque el parpadeo "quisiera" subir.
        if (g_inBlackout)
            flashlightFlicker = 0.0f;

        ourShader.setFloat("flashlightFlicker", flashlightFlicker);

        // Palpitar tipo latido para las luces rojas del techo, pero cada
        // lampara con su PROPIO desfase (golden angle) y su propio parpadeo
        // ocasional -- que no esten todas sincronizadas da mucho mas
        // ambiente de "instalacion electrica fallando" que un pulso unico.
        const float GOLDEN_ANGLE = 2.399963f;
        if (g_redLightWasOff.size() != g_redLightPositions.size())
            g_redLightWasOff.assign(g_redLightPositions.size(), false);

        std::vector<float> perLightIntensity(g_redLightPositions.size());
        for (size_t i = 0; i < g_redLightPositions.size(); i++)
        {
            float phase = (float)i * GOLDEN_ANGLE;

            // Fluctuacion normal: dos ondas de frecuencias no relacionadas
            // sumadas (no una sola sinusoide perfecta) para que el "respirar"
            // se sienta irregular, como un tubo fluorescente inestable.
            float waveSlow = 0.5f + 0.5f * sinf(currentFrame * RED_LIGHT_PULSE_SPEED + phase);
            float waveFast = 0.5f + 0.5f * sinf(currentFrame * RED_LIGHT_PULSE_SPEED * 5.3f + phase * 1.7f);
            float combinedWave = waveSlow * 0.8f + waveFast * 0.2f;
            float value = RED_LIGHT_BASE_INTENSITY + RED_LIGHT_PULSE_AMOUNT * combinedWave;

            // -------- Ciclo propio de esta lampara: normal -> se pone
            // inestable -> se corta -> vuelve titilando -> normal --------
            float period = PseudoRandomRange((float)i * 12.11f + 1.0f, RED_LIGHT_OFF_PERIOD_MIN, RED_LIGHT_OFF_PERIOD_MAX);
            float offDuration = PseudoRandomRange((float)i * 27.53f + 2.0f, RED_LIGHT_OFF_DURATION_MIN, RED_LIGHT_OFF_DURATION_MAX);
            float phaseOffset = PseudoRandomRange((float)i * 41.77f + 3.0f, 0.0f, period);
            float cyclePos = fmodf(currentFrame + phaseOffset, period);

            const float PRE_OFF_STUTTER = 1.4f;  // se pone inestable antes de cortarse
            const float POST_ON_STUTTER = 1.1f;  // titila al volver, antes de estabilizarse
            const float STUTTER_RATE = 13.0f;    // "parpadeos" por segundo durante la inestabilidad

            bool isOff = cyclePos < offDuration;
            bool isPreOffStutter = cyclePos >= (period - PRE_OFF_STUTTER);
            bool isPostOnStutter = !isOff && cyclePos < (offDuration + POST_ON_STUTTER);

            if (isPreOffStutter || isPostOnStutter)
            {
                // Parpadeo brusco tipo foco flojo/a punto de fallar: bloques
                // rapidos de prendido/apagado, no un fundido suave.
                float stutterStep = floorf(currentFrame * STUTTER_RATE + (float)i * 3.7f);
                float stutterRoll = PseudoRandom01(stutterStep * 7.91f + (float)i * 101.3f);
                value = (stutterRoll > 0.45f) ? value : value * 0.05f;
            }

            if (isOff != g_redLightWasOff[i])
            {
                std::cout << "[DEBUG] Luz roja " << i << (isOff ? " se APAGO (corte de electricidad)" : " volvio la electricidad") << std::endl;
                g_redLightWasOff[i] = isOff;
            }
            if (isOff)
                value = 0.0f;

            // Parpadeo brusco ocasional adicional, sutil, incluso en operacion normal
            float lightFlickerStep = floorf(currentFrame / (FLICKER_STEP_DURATION * 4.0f));
            float lightFlickerRoll = PseudoRandom01(lightFlickerStep * 5.13f + (float)i * 91.7f);
            if (!isOff && lightFlickerRoll < 0.06f)
                value *= 0.15f;

            perLightIntensity[i] = value;
        }

        ourShader.setVec3("redLightColor", RED_LIGHT_COLOR);
        ourShader.setInt("numRedLights", (int)g_redLightPositions.size());
        for (size_t i = 0; i < g_redLightPositions.size(); i++)
        {
            ourShader.setVec3("redLightPositions[" + std::to_string(i) + "]", g_redLightPositions[i]);
            ourShader.setFloat("redLightIntensities[" + std::to_string(i) + "]", perLightIntensity[i]);
        }

        // Niebla, vineta y gradacion de color
        ourShader.setVec3("fogColor", FOG_COLOR);
        ourShader.setFloat("fogDensity", FOG_DENSITY);
        ourShader.setVec2("screenSize", glm::vec2((float)SCR_WIDTH, (float)SCR_HEIGHT));
        ourShader.setFloat("desaturation", DESATURATION_AMOUNT);
        ourShader.setFloat("materialShininess", MATERIAL_SHININESS);
        ourShader.setFloat("uTime", currentFrame);
        ourShader.setFloat("contrastPower", CONTRAST_POWER);
        ourShader.setFloat("grainAmount", GRAIN_AMOUNT);

        // Golpe visual: fuerte al instante del corte, se desvanece rapido
        float startleElapsed = currentFrame - g_startleTriggerTime;
        float startleFlash = (startleElapsed >= 0.0f && startleElapsed < STARTLE_FLASH_DURATION)
            ? (1.0f - startleElapsed / STARTLE_FLASH_DURATION)
            : 0.0f;
        ourShader.setFloat("startleFlash", startleFlash);

        ourModel.Draw(ourShader);

        // El modelo extra usa el MISMO shader ya configurado este frame
        // (linterna, luces rojas, niebla, grano, etc.) -- solo cambiamos
        // la matriz "model" para ubicarlo en su lugar.
        if (extraModel)
        {
            ourShader.setMat4("model", extraModelMatrix);
            extraModel->Draw(ourShader);
        }

        // -------- Bombillas visibles en cada foco de techo --------
        // Sirve tanto para atmosfera (se ve un punto de luz real) como
        // para depurar: si el punto no queda pegado a una lampara real
        // del modelo, hay que ajustar manualLightMeshIndices.
        glm::vec3 camRight = glm::normalize(glm::cross(camera.Front, camera.WorldUp));
        glm::vec3 camUp = glm::normalize(glm::cross(camRight, camera.Front));

        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        bulbShader.use();
        bulbShader.setMat4("view", view);
        bulbShader.setMat4("projection", projection);
        bulbShader.setVec3("camRight", camRight);
        bulbShader.setVec3("camUp", camUp);
        bulbShader.setVec3("billboardColor", RED_LIGHT_COLOR);
        bulbShader.setFloat("billboardScale", LIGHT_BULB_SCALE);

        glBindVertexArray(bulbVAO);
        for (size_t i = 0; i < g_redLightPositions.size(); i++)
        {
            bulbShader.setVec3("billboardCenter", g_redLightPositions[i]);
            bulbShader.setFloat("billboardIntensity", perLightIntensity[i] * 1.3f);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        glBindVertexArray(0);

        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);

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