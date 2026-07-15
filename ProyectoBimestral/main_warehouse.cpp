// ============================================================
// main.cpp - Warehouse con colision por GRILLA 2D basada en
// triangulos reales (no bounding boxes por objeto). Esto respeta
// huecos/aberturas del modelo y evita "pegarse" a las paredes.
// ============================================================

#define NOMINMAX

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>
#include <learnopengl/stb_image.h>
#include <learnopengl/audio.h>

#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <cfloat>
#include <cmath>

extern enum SceneType { SCENE_PASILLO, SCENE_SAMY, SCENE_ANI, SCENE_MATTHEW, SCENE_JOSUE };
extern SceneType g_CurrentScene;
extern SceneType g_NextScene;
extern int g_UnlockedLevel;

namespace Warehouse {

    // Callbacks / funciones auxiliares
    void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    void mouse_callback(GLFWwindow* window, double xpos, double ypos);
    void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    void processInput(GLFWwindow* window);

    // settings
    static unsigned int SCR_WIDTH = 1280;
    static unsigned int SCR_HEIGHT = 720;

    // -----------------------------------------------------------
    // Modelo extra: se carga, escala y ubica cerca (no exacto) del centro
    // del warehouse, a nivel de piso, y se dibuja con el MISMO shader que
    // el resto (hereda linterna, luces rojas, niebla, grano, etc. solo).
    // NOTA: Se desactiva ya que el archivo scene.gltf del árbol no está en el repo.
    // -----------------------------------------------------------
    static const bool ADD_EXTRA_MODEL = true;
    static const std::string EXTRA_MODEL_PATH = "./model/C_Tree/C_tree.obj";
    static const float EXTRA_MODEL_TARGET_HEIGHT = 4.0f; // altura deseada en unidades de mundo, ajustable
    static const bool  EXTRA_MODEL_IS_Z_UP = false;       // poner true si aparece "acostado"
    static const bool  EXTRA_MODEL_ADD_COLLISION = true;  // false si no queres que bloquee el paso

    // 0.0 = centro exacto del edificio, 1.0 = justo en el punto de spawn.
    // Bajado para que quede mas cerca del centro real del edificio.
    static const float EXTRA_MODEL_TOWARD_SPAWN_BLEND = 0.15f;
    // Margen extra sobre el radio estimado del arbol al buscar un lugar
    // libre -- evita que quede pegado o superpuesto a una silla/mesa.
    static const float EXTRA_MODEL_COLLISION_MARGIN = 0.5f;

    // -----------------------------------------------------------
    // Bombillos de colores tipo arbol de navidad: se intenta usar los
    // bombillos REALES del modelo (mallas chicas y compactas, tipo esfera,
    // repartidas por el follaje), detectados automaticamente por tamano y
    // forma. Si algun bombillo real queda mal detectado, se puede forzar
    // a mano con manualOrnamentMeshIndices (mirando el listado de consola
    // "[ARBOL MESH i]"). Si no se detecta nada, cae a un reparto aleatorio
    // dentro del volumen del arbol como ultimo recurso.
    // -----------------------------------------------------------
    static const bool  EXTRA_MODEL_ORNAMENT_LIGHTS = true;
    static const int   ORNAMENT_LIGHT_COUNT = 10;          // tope de bombillos a usar (<= MAX_ORNAMENT_LIGHTS del shader)
    static const float ORNAMENT_LIGHT_BASE_INTENSITY = 1.3f;
    static const float ORNAMENT_LIGHT_BILLBOARD_SCALE = 0.22f;
    static const float ORNAMENT_MAX_FOOTPRINT = 0.5f;      // una bombilla real es chica en las 3 dimensiones
    static const float ORNAMENT_MAX_ASPECT_RATIO = 2.2f;   // descarta ramas/hojas alargadas
    static std::vector<int> manualOrnamentMeshIndices = { };

    // -----------------------------------------------------------
    // Modelos repetidos alrededor del arbol, en forma de X (4 puntas). Se
    // carga el archivo UNA sola vez y se dibuja varias veces con distinta
    // matriz de posicion (mucho mas liviano que cargarlo N veces). Usa el
    // mismo shader que todo lo demas, asi que hereda linterna, luces
    // rojas, niebla, grano, etc. automaticamente.
    // -----------------------------------------------------------
    static const bool  ADD_REPEATED_MODEL = true;
    static const std::string REPEATED_MODEL_PATH = "./model/Zombie_torso/zombie.obj";
    static const float REPEATED_MODEL_TARGET_HEIGHT = 1.6f; // altura aproximada, ajustable
    static const bool  REPEATED_MODEL_IS_Z_UP = false;
    static const bool  REPEATED_MODEL_ADD_COLLISION = true;
    static const int   REPEATED_MODEL_COUNT = 4;            // 4 puntas de la X
    static const float REPEATED_MODEL_RADIUS_FROM_TREE = 2.6f; // mas cerca del arbol, lejos de columnas

    // Temblor/sacudida de los zombies: saltos bruscos a un nuevo angulo cada
    // tanto (no una animacion suave), como un cuerpo roto/rigido. Subido
    // fuerte porque el modelo es solo el torso (sin cabeza/brazos separados
    // para animar aparte), asi que el movimiento del cuerpo completo tiene
    // que notarse por si solo.
    static const float ZOMBIE_TWITCH_YAW_DEG = 22.0f;      // cuanto puede "saltar" de angulo
    static const float ZOMBIE_TWITCH_STEP_DURATION = 0.9f; // cada cuanto cambia el angulo
    static const float ZOMBIE_TWITCH_BOB_AMOUNT = 0.09f;   // amplitud del vaiven vertical
    static const float ZOMBIE_TWITCH_BOB_SPEED = 2.4f;

    // Vaiven de POSICION (se tambalea en el lugar, no solo gira). El radio
    // se mantiene chico a proposito para no salirse del margen de colision
    // ya reservado al ubicarlo (REPEATED_MODEL_COLLISION_MARGIN).
    static const float ZOMBIE_SWAY_RADIUS = 0.22f;
    static const float ZOMBIE_SWAY_SPEED = 0.75f;

    // Inclinacion lateral tipo "a punto de caerse", ademas del giro
    static const float ZOMBIE_LEAN_DEG = 8.0f;
    static const float ZOMBIE_LEAN_SPEED = 0.55f;
    static const glm::vec3 ZOMBIE_LEAN_AXIS = glm::vec3(0.0f, 0.0f, 1.0f); // ajustar a (1,0,0) si la inclinacion queda "para adelante/atras" en vez de "de lado"

    // -----------------------------------------------------------
    // Movimiento de cabeza y brazos: como el .obj no tiene huesos, esto solo
    // funciona si el modelo tiene la cabeza y los brazos como MALLAS
    // SEPARADAS (algo muy comun en modelos de Sketchfab). Se detectan
    // automaticamente por posicion relativa al resto del cuerpo:
    // - Cabeza: arriba de todo, cerca del centro horizontal.
    // - Brazo: a media altura, notablemente corrido del centro horizontal.
    // Si la deteccion falla, forzar a mano con manualZombieHeadMeshIndices /
    // manualZombieArmMeshIndices, mirando el listado "[ZOMBIE MESH i]" de
    // la consola. Si un eje de giro queda "para el lado que no es", ajustar
    // ZOMBIE_HEAD_SWAY_AXIS / ZOMBIE_ARM_SWING_AXIS (ej. de (1,0,0) a (0,0,1)).
    // -----------------------------------------------------------
    static const bool  ZOMBIE_ANIMATE_PARTS = true;
    static const float ZOMBIE_HEAD_HEIGHT_FRAC_MIN = 0.78f;  // debe estar en el 22% superior del cuerpo
    static const float ZOMBIE_HEAD_MAX_OFFSET_FRAC = 0.3f;   // debe estar cerca del centro horizontal
    static const float ZOMBIE_ARM_HEIGHT_FRAC_MIN = 0.35f;   // rango de altura tipo "hombro/brazo"
    static const float ZOMBIE_ARM_HEIGHT_FRAC_MAX = 0.85f;
    static const float ZOMBIE_ARM_MIN_OFFSET_FRAC = 0.28f;   // debe estar corrido del centro (no en el medio)

    static const float ZOMBIE_HEAD_SWAY_DEG = 16.0f;
    static const float ZOMBIE_HEAD_SWAY_SPEED = 0.9f;
    static const glm::vec3 ZOMBIE_HEAD_SWAY_AXIS = glm::vec3(0.0f, 1.0f, 0.0f); // Y = girar la cabeza de lado a lado

    static const float ZOMBIE_ARM_SWING_DEG = 28.0f;
    static const float ZOMBIE_ARM_SWING_SPEED = 0.6f;
    static const glm::vec3 ZOMBIE_ARM_SWING_AXIS = glm::vec3(1.0f, 0.0f, 0.0f); // X = subir/bajar el brazo hacia adelante

    static std::vector<int> manualZombieHeadMeshIndices = { };
    static std::vector<int> manualZombieArmMeshIndices = { };
    static const float REPEATED_MODEL_COLLISION_MARGIN = 0.4f;
    static const bool  REPEATED_MODEL_FACE_CENTER = true;   // gira cada instancia mirando hacia el arbol
    static const float REPEATED_MODEL_YAW_OFFSET_DEG = 0.0f; // ajustar +-90/180 si quedan mirando al reves

    // -----------------------------------------------------------
    // NOTA DE DISENO: como el zombie mas cercano se puede mover (ver
    // abajo), NINGUNO de los 4 tiene colision fisica fija -- si la
    // tuvieran, quedaria un "muro fantasma" en su posicion original
    // cuando el que se mueve se aleje. El jugador puede atravesar
    // visualmente a los que estan quietos; el "gotcha" real es el
    // jumpscare al acercarse o al mirarlos fijo, no un choque fisico.
    // -----------------------------------------------------------

    // Jumpscare por mirada sostenida: mirar fijo a CUALQUIER zombie por
    // varios segundos dispara una imagen de miedo a pantalla completa +
    // flash rojo + linterna estroboscopica + golpe de zoom. Con
    // enfriamiento global para que no se repita todo el tiempo.
    static const bool  JUMPSCARE_STARE_ENABLED = true;
    static const float JUMPSCARE_STARE_DURATION = 1.6f;   // segundos mirando fijo para disparar (bajado de 3.0)
    static const float JUMPSCARE_STARE_MAX_DISTANCE = 9.0f;
    static const float JUMPSCARE_STARE_FOV_ANGLE_DEG = 12.0f;
    static const float JUMPSCARE_EFFECT_DURATION = 0.8f; // cuanto dura el golpe (flash+zoom+estrobo+shake)
    static const float JUMPSCARE_IMAGE_DURATION = 0.95f;  // cuanto dura la imagen en pantalla (negro+estrobo+sostenido+corte)
    static const float JUMPSCARE_COOLDOWN = 9.0f;         // bajado de 22: que pueda repetirse mas seguido
    static const float JUMPSCARE_ZOOM_PUNCH = 34.0f;      // subido: golpe de zoom mas brusco
    static const std::string JUMPSCARE_IMAGE_PATH = "./model/exit/scream.jpeg";

    // -----------------------------------------------------------
    // Sonido. Se usan los assets que ya tiene el equipo:
    // - music/scream.mp3      -> grito en el jumpscare
    // - music/radio.mp3       -> estatica/interferencia cuando la linterna
    //                            falla y cuando vuelve
    // - music/suspenso.mp3    -> musica ambiental de fondo (loop)
    // - audios/miedo.mp3      -> sonido puntual de "alguien te sigue",
    //                            solo suena mientras el perseguidor esta activo
    // Se precargan con alias propios (preloadSound/playPreloaded) para que
    // puedan sonar EN SIMULTANEO sin cortarse entre si (a diferencia de
    // playsound(), que solo soporta un sonido a la vez).
    // -----------------------------------------------------------
    static const std::string SOUND_SCREAM_PATH = "music/scream.mp3";
    static const std::string SOUND_LIGHT_FX_PATH = "music/radio.mp3";
    // Musica de fondo: la pista navideña, en loop desde que carga la escena.
    static const std::string SOUND_AMBIENT_MUSIC_PATH = "music/silent-night.mp3";
    // Sonido especifico del apagon de las luces del TECHO (no la linterna).
    static const std::string SOUND_CEILING_BLACKOUT_PATH = "music/falling-power-sound.mp3";
    static const std::string SOUND_PRESENCE_PATH = "audios/miedo.mp3";
    static const float PRESENCE_SOUND_MIN_INTERVAL = 14.0f;
    static const float PRESENCE_SOUND_MAX_INTERVAL = 28.0f;
    static float g_nextPresenceSoundTime = -1.0f;

    // Persecucion tipo "no me mires": DESPUES de pasar el arbol, el
    // zombie MAS CERCANO al jugador se acerca despacio mientras no lo
    // estes mirando, y se congela de golpe en el instante en que lo
    // volves a ver. Esquiva paredes/columnas/el arbol con el mismo
    // sistema de colision que el jugador. Si te alcanza sin haberlo
    // visto, dispara el mismo jumpscare que la mirada sostenida.
    static const bool  ZOMBIE_APPROACH_ENABLED = true;
    static const float ZOMBIE_APPROACH_SPEED = 2.3f;             // rapido: real amenaza si no lo mirás
    static const float ZOMBIE_APPROACH_LOOK_ANGLE_DEG = 25.0f;   // cono generoso: facil "volver a verlo" y congelarlo
    static const float ZOMBIE_APPROACH_LOOK_MAX_DISTANCE = 40.0f; // cubre casi toda la sala
    static const float ZOMBIE_APPROACH_STOP_DISTANCE = 3.4f;     // mas rango: ya no queda pegado a la camara
    static const float ZOMBIE_APPROACH_ATTACK_DISTANCE = 1.1f;   // si llega aca sin ser visto, jumpscare

    // "Territorio": no te persigue infinito. Si te alejas mas de esto
    // desde SU punto de spawn original (cerca del arbol), se frena y
    // queda quieto ahi -- como llegando "un poco antes de la puerta y
    // no mas". Si volves a acercarte, retoma la persecucion.
    static const float ZOMBIE_APPROACH_MAX_RANGE = 20.0f;

    // Embestida: si te quedas SIN mirarlo mientras esta relativamente cerca
    // (dentro de ENGAGE_RANGE) por varios segundos seguidos, cierra la
    // distancia de golpe hasta la de ataque y dispara el jumpscare -- asi
    // "guarda distancia" normalmente pero igual te puede alcanzar si lo
    // ignoras del todo. Despues de atacar, se retira a su posicion
    // original y espera un rato antes de volver a acercarse -- asi el
    // efecto de persecucion se repite en vez de terminar una sola vez.
    static const float ZOMBIE_LUNGE_ENGAGE_RANGE = 9.0f;
    static const float ZOMBIE_LUNGE_THRESHOLD = 1.6f;     // bajado de 4.0: embiste mucho mas seguido
    static const float ZOMBIE_RETREAT_GRACE_DURATION = 5.0f;
    static float g_pursuerLungeTimer = 0.0f;
    static float g_pursuerRetreatUntil = -1000.0f;

    // Aviso de cercania: suena UNA vez cada vez que cruza de "lejos" a
    // "cerca" (no en bucle), distinto del jumpscare que es al tocarte.
    static const float ZOMBIE_WARNING_RANGE = 5.5f;
    static bool g_pursuerWasNear = false;

    // ---------------------------------------------------------
    // Configuración de escala / jugador
    // ---------------------------------------------------------
    static const float TARGET_BUILDING_SIZE = 40.0f;
    static const float PLAYER_EYE_HEIGHT = 1.7f;

    // -----------------------------------------------------------
    // Esquina de spawn: 0.0 = pegado a la pared del minimo de ese eje,
    // 1.0 = pegado a la pared del maximo, 0.5 = centro. Para spawnear en
    // la esquina OPUESTA a la puerta, probar las 4 combinaciones (0,0),
    // (1,0), (0,1), (1,1) y quedarse con la que quede lejos de la puerta
    // (no sabemos de este lado del codigo donde esta la puerta exactamente).
    // -----------------------------------------------------------
    static const float SPAWN_X_FRAC = 0.1f;
    static const float SPAWN_Z_FRAC = 0.1f;
    static const float PLAYER_WALK_SPEED = 3.0f;
    static const float PLAYER_RADIUS = 0.3f;
    static const bool  MODEL_IS_Z_UP = false;

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
    static const float COLLISION_BAND_MIN = 0.35f; // por encima del piso
    static const float COLLISION_BAND_MAX = 2.2f;  // hasta un poco mas alto que la cabeza

    // -----------------------------------------------------------
    // Respaldo adicional a nivel de malla completa: si TODA una malla
    // (cable, hoja, libreta) es chata y esta pegada al piso, se excluye
    // entera aunque alguno de sus triangulos rozara la franja de arriba.
    // -----------------------------------------------------------
    static const float FLOOR_CLUTTER_MAX_HEIGHT = 0.3f;
    static const float FLOOR_CLUTTER_MAX_ELEVATION = 0.4f;

    // Si es true, imprime en consola el indice, tamano y posicion de
    // cada malla al iniciar. Sirve para identificar cables colgantes
    // u otros objetos que quieras excluir a mano en excludedMeshIndices.
    static const bool PRINT_MESH_DEBUG = true;

    // Tamano de celda de la grilla de colision (mas chico = mas preciso, mas memoria)
    static const float GRID_CELL_SIZE = 0.4f;

    // -----------------------------------------------------------
    // Mascara de "hay piso aca". Un triangulo cuenta como piso si su
    // punto mas bajo esta cerca del nivel del piso general (admite
    // pequenas rampas/escalones). Se usa para impedir salir del molde
    // real del piso, en vez de depender de un margen artificial.
    // -----------------------------------------------------------
    static const float FLOOR_MASK_MAX_ELEVATION = 0.5f;

    // Margen alrededor del edificio para poder salir por aberturas/puertas
    // y caminar un poco por el exterior sin chocar con un limite artificial.
    static const float GRID_SAFETY_MARGIN = 8.0f;

    // -----------------------------------------------------------
    // Iluminacion de terror: linterna SIEMPRE encendida (con parpadeo de
    // bateria vieja) + un glow rojo de fondo siempre presente y tenue +
    // niebla que come la visibilidad lejana + vineta en los bordes.
    // Todo combinado a la vez, no es un modo que se elige.
    // -----------------------------------------------------------
    static const float FLASHLIGHT_INNER_CUTOFF_DEG = 13.0f;
    static const float FLASHLIGHT_OUTER_CUTOFF_DEG = 21.0f;

    // Parpadeo de linterna: cada FLICKER_STEP_DURATION segundos se decide un
    // nuevo nivel de intensidad. La mayoria de las veces es sutil (0.75-1.0);
    // ocasionalmente hay un "corte" fuerte simulando mal contacto de bateria.
    static const float FLICKER_STEP_DURATION = 0.18f;
    static const float FLICKER_DIP_CHANCE = 0.10f;   // probabilidad de corte fuerte por paso
    static const float FLICKER_DIP_MIN = 0.08f;      // que tan oscuro llega a quedar en un corte

    // -----------------------------------------------------------
    // Apagon real: cada tanto la linterna se va a negro por completo
    // durante un ratito, distinto del parpadeo rapido de arriba (que es
    // un temblor sutil constante). Esto es "la bateria fallo en serio".
    // El proximo apagon se sortea dentro de [MIN_INTERVAL, MAX_INTERVAL]
    // segundos despues del anterior, y dura entre [MIN_DURATION, MAX_DURATION].
    // -----------------------------------------------------------
    static const float BLACKOUT_MIN_INTERVAL = 12.0f;
    static const float BLACKOUT_MAX_INTERVAL = 28.0f;
    static const float BLACKOUT_MIN_DURATION = 1.0f;
    static const float BLACKOUT_MAX_DURATION = 3.0f;

    static const glm::vec3 RED_LIGHT_COLOR = glm::vec3(1.0f, 0.05f, 0.05f);
    static const float RED_LIGHT_BASE_INTENSITY = 1.3f;   // subido: ahora se nota bien incluso lejos
    static const float RED_LIGHT_PULSE_AMOUNT = 0.9f;     // rango grande de sube-y-baja
    static const float RED_LIGHT_PULSE_SPEED = 0.55f;     // mas lento: ciclo completo dura ~11s, se percibe como "respirar"

    // Niebla: cuanto mas alto FOG_DENSITY, antes se pierde la visibilidad.
    // Mas oscura y densa que antes: el fondo se pierde en negro casi total.
    static const glm::vec3 FOG_COLOR = glm::vec3(0.012f, 0.003f, 0.003f);
    static const float FOG_DENSITY = 0.012f;

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
    static const bool  USE_AUTO_DETECTED_CEILING_LIGHTS = false; // recomendado: false (usar grilla)

    static const float CEILING_LIGHT_MAX_FOOTPRINT = 1.5f;
    static const float CEILING_LIGHT_MIN_ELEVATION_FRAC = 0.90f;
    static const float CEILING_LIGHT_MIN_WALL_MARGIN = 1.5f;
    static const float CEILING_LIGHT_MAX_ASPECT_RATIO = 2.0f;

    static const int   CEILING_LIGHT_GRID_COLS = 3;
    static const int   CEILING_LIGHT_GRID_ROWS = 3;
    static const float CEILING_LIGHT_GRID_MARGIN_FRAC = 0.15f; // que tan adentro de las paredes arranca la grilla

    static const int   MAX_RED_LIGHTS = 9; // 3x3
    static std::vector<int> manualLightMeshIndices = { };

    // Tamano visual de la "bombilla" que se dibuja en cada foco (unidades de mundo)
    static const float LIGHT_BULB_SCALE = 0.6f;

    // Gradacion de color: la parte "neutra" de la escena (ambiente + linterna)
    // se desatura hacia gris, pero el rojo de las lamparas queda intacto y
    // bien saturado -- asi el rojo resalta mucho mas contra todo lo demas.
    static const float DESATURATION_AMOUNT = 0.55f;

    // Brillo especular: mas alto = destellos mas chicos y nitidos (superficie
    // muy lisa/metalica/mojada); mas bajo = brillo mas difuso y amplio.
    static const float MATERIAL_SHININESS = 24.0f;

    // Contraste crudo: >1 aplasta las sombras a negro (no se "adivina" lo que
    // hay en la oscuridad); 1.0 = sin cambio. Grano: textura sucia tipo
    // camara vieja/found footage, 0 = sin grano.
    static const float CONTRAST_POWER = 1.45f;
    static const float GRAIN_AMOUNT = 0.085f;

    // Indices de malla a excluir por completo de la colision (por ejemplo
    // una lampara colgante, cables, o el "shell" exterior/techo si genera
    // falsos positivos). Se llenan mirando la consola si algo bloquea mal.
    static std::vector<int> excludedMeshIndices = { };

    // ---------------------------------------------------------
    // Cámara
    // ---------------------------------------------------------
    static Camera camera(glm::vec3(0.0f, 2.0f, 8.0f));
    static float lastX = 1280.0f / 2.0f;
    static float lastY = 720.0f / 2.0f;
    static bool firstMouse = true;

    // ---------------------------------------------------------
    // Timing
    // ---------------------------------------------------------
    static float deltaTime = 0.0f;
    static float lastFrame = 0.0f;

    // Estado del apagon real de la linterna
    static bool g_inBlackout = false;
    static float g_nextBlackoutTime = -1.0f; // se inicializa la primera vez que se usa
    static float g_blackoutEndTime = 0.0f;

    // Golpe visual: momento en que se disparo el ultimo flash (para el corte
    // de linterna). -1000 = nunca, asi el primer frame no dispara nada.
    static float g_startleTriggerTime = -1000.0f;
    static const float STARTLE_FLASH_DURATION = 0.28f;

    // Estado del jumpscare (imagen a pantalla completa)
    static float g_jumpscareTriggerTime = -1000.0f;
    static float g_jumpscareCooldownUntil = -1000.0f;
    static bool  g_jumpscareActive = false; // true durante la ventana de efecto (flash/zoom/estrobo)
    static float g_jumpscareActiveEndTime = -1000.0f;
    static std::vector<float> g_zombieGazeTimer;

    // Posicion/orientacion ACTUAL de cada zombie (persiste entre frames;
    // solo el mas cercano se mueve, y solo despues de pasar el arbol).
    static std::vector<glm::vec2> g_zombieCurrentXZ;
    static std::vector<float> g_zombieMoveYaw;

    // Perseguidor FIJO: una vez elegido, sigue siendo el mismo zombie el
    // que te persigue durante todo el cruce de la sala (no se recalcula
    // "el mas cercano" cada frame, porque eso repartia el avance entre
    // los 4 y ninguno llegaba a ningun lado). -1 = todavia no elegido.
    static int g_pursuerIndex = -1;

    // Sacudida de camara durante el jumpscare
    static const float JUMPSCARE_SHAKE_AMOUNT = 0.32f;

    // Apagon real por foco: cada luz roja tiene su propio ciclo de
    // encendido/apagado (periodo y duracion del apagon distintos entre si,
    // para que no se apaguen todas juntas). Es deterministico en base al
    // tiempo, no necesita guardar estado -- salvo un vector para detectar
    // la transicion y loguearla en consola.
    static const float RED_LIGHT_OFF_PERIOD_MIN = 28.0f;   // cada cuanto (min) le toca un apagon a una luz
    static const float RED_LIGHT_OFF_PERIOD_MAX = 45.0f;   // cada cuanto (max)
    static const float RED_LIGHT_OFF_DURATION_MIN = 12.0f; // cuanto dura el apagon (min) -- largo, para que se note bien
    static const float RED_LIGHT_OFF_DURATION_MAX = 18.0f; // cuanto dura el apagon (max)
    static std::vector<bool> g_redLightWasOff;
    static std::vector<bool> g_redLightWasPreStutter;

    enum ExitPhase { EXIT_NONE, EXIT_WAIT_BLACK, EXIT_FADE_IN, EXIT_SHOW, EXIT_FADE_OUT };
    static ExitPhase exitPhase = EXIT_NONE;
    static float exitTimer = 0.0f;
    static const std::string MATT_MESSAGE_IMAGE_PATH = "./model/Warehouse/abandoned_warehouse_-_interior_scene/message_matt.jpeg";

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

    static CollisionGrid g_grid;      // 1 = bloqueado (pared, columna, silla, etc.)
    static CollisionGrid g_floorMask; // 1 = hay piso real debajo

    static std::vector<glm::vec3> g_meshCenters;         // centro (mundo) de cada malla, indexado igual que model.meshes
    static std::vector<int> g_autoDetectedLightIndices;  // indices de mallas candidatas a "foco de techo"
    static std::vector<glm::vec3> g_redLightPositions;   // posiciones finales usadas por el shader

    static std::vector<glm::vec3> g_ornamentLightPositions;
    static std::vector<glm::vec3> g_ornamentLightColors;

    static const glm::vec3 ORNAMENT_PALETTE[] = {
        glm::vec3(1.0f, 0.12f, 0.10f), // rojo
        glm::vec3(0.15f, 1.0f, 0.20f), // verde
        glm::vec3(0.20f, 0.35f, 1.0f), // azul
        glm::vec3(1.0f, 0.85f, 0.10f), // amarillo
        glm::vec3(1.0f, 0.20f, 0.75f), // rosa/magenta
        glm::vec3(0.15f, 0.95f, 0.95f) // cian
    };
    static const int ORNAMENT_PALETTE_SIZE = 6;

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
            float footprint = (std::max)(footprintX, footprintZ);

            // Distancia horizontal a la pared exterior mas cercana (usa el AABB
            // completo del edificio como aproximacion del perimetro). Un dintel
            // de puerta esta pegado a la pared -> distancia chica. Una lampara
            // que cuelga del techo suele estar mas hacia adentro.
            float distToWallX = (std::min)(meshCenter.x - worldBounds.min.x, worldBounds.max.x - meshCenter.x);
            float distToWallZ = (std::min)(meshCenter.z - worldBounds.min.z, worldBounds.max.z - meshCenter.z);
            float distToNearestWall = (std::min)(distToWallX, distToWallZ);

            // Aspect ratio: un dintel/viga es angosto y largo (aspect ratio alto);
            // una lampara es mas pareja en ambos ejes horizontales.
            float aspectRatio = (std::max)(footprintX, footprintZ) / (std::max)((std::min)(footprintX, footprintZ), 0.01f);

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
                    << "\n";
            }

            if (excludedManually)
                continue;

            for (size_t i = 0; i + 2 < idx.size(); i += 3)
            {
                trianglesTotal++;

                glm::vec3 p0 = glm::vec3(modelMatrix * glm::vec4(verts[idx[i]].Position, 1.0f));
                glm::vec3 p1 = glm::vec3(modelMatrix * glm::vec4(verts[idx[i + 1]].Position, 1.0f));
                glm::vec3 p2 = glm::vec3(modelMatrix * glm::vec4(verts[idx[i + 2]].Position, 1.0f));

                float yMin = (std::min)({ p0.y, p1.y, p2.y });
                float yMax = (std::max)({ p0.y, p1.y, p2.y });

                float xMin = (std::min)({ p0.x, p1.x, p2.x });
                float xMax = (std::max)({ p0.x, p1.x, p2.x });
                float zMin = (std::min)({ p0.z, p1.z, p2.z });
                float zMax = (std::max)({ p0.z, p1.z, p2.z });

                // Mascara de piso
                if (yMin - floorY < FLOOR_MASK_MAX_ELEVATION)
                    g_floorMask.MarkSolidWorldBox(xMin, xMax, zMin, zMax);

                // Colision
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
            std::cout << "[DEBUG] Usando " << g_redLightPositions.size() << " foco(s) definidos manualmente." << std::endl;
        }
        else if (USE_AUTO_DETECTED_CEILING_LIGHTS && !g_autoDetectedLightIndices.empty())
        {
            for (int idx : g_autoDetectedLightIndices)
            {
                if ((int)g_redLightPositions.size() >= MAX_RED_LIGHTS) break;
                g_redLightPositions.push_back(g_meshCenters[idx]);
            }
            std::cout << "[DEBUG] " << g_autoDetectedLightIndices.size() << " candidato(s) auto, usando "
                << g_redLightPositions.size() << "." << std::endl;
        }
        else
        {
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
            std::cout << "[DEBUG] Usando grilla de " << g_redLightPositions.size() << " focos." << std::endl;
        }
    }

    // -----------------------------------------------------------
    // Suma la colision de un modelo EXTRA (ej. el arbol) a la grilla
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

                float yMin = (std::min)({ p0.y, p1.y, p2.y });
                float yMax = (std::max)({ p0.y, p1.y, p2.y });
                if (yMax < bandMin || yMin > bandMax)
                    continue;

                float xMin = (std::min)({ p0.x, p1.x, p2.x });
                float xMax = (std::max)({ p0.x, p1.x, p2.x });
                float zMin = (std::min)({ p0.z, p1.z, p2.z });
                float zMax = (std::max)({ p0.z, p1.z, p2.z });

                g_grid.MarkSolidWorldBox(xMin, xMax, zMin, zMax);
                trianglesAdded++;
            }
        }
        std::cout << "[DEBUG] Colision agregada para modelo extra: " << trianglesAdded << " triangulos." << std::endl;
    }

    // Hay piso pisable en esta posicion?
    bool HasFloorAt(const glm::vec3& pos)
    {
        return g_floorMask.IsSolidWorldPos(pos.x, pos.z);
    }

    // Revisa colision circulo
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

    // Carga una textura 2D simple desde disco (para la imagen del jumpscare,
    // que no pasa por la clase Model ya que es una imagen suelta, no un OBJ).
    unsigned int LoadTexture2D(const std::string& path)
    {
        unsigned int textureID;
        glGenTextures(1, &textureID);

        int w, h, channels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 0);
        if (data)
        {
            GLenum format = GL_RGB;
            if (channels == 1) format = GL_RED;
            else if (channels == 3) format = GL_RGB;
            else if (channels == 4) format = GL_RGBA;

            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            stbi_image_free(data);
            std::cout << "[DEBUG] Textura de jumpscare cargada: " << path << " (" << w << "x" << h << ")" << std::endl;
        }
        else
        {
            std::cout << "[DEBUG] ERROR: no se pudo cargar la textura de jumpscare en " << path << std::endl;
            stbi_image_free(data);
        }
        stbi_set_flip_vertically_on_load(false);
        return textureID;
    }

    // Movimiento con sliding
    glm::vec3 TryMove(const glm::vec3& currentPos, const glm::vec3& delta)
    {
        glm::vec3 newPos = currentPos;

        glm::vec3 testX = newPos + glm::vec3(delta.x, 0.0f, 0.0f);
        if (!CollidesAt(testX) && HasFloorAt(testX)) newPos.x = testX.x;

        glm::vec3 testZ = newPos + glm::vec3(0.0f, 0.0f, delta.z);
        if (!CollidesAt(testZ) && HasFloorAt(testZ)) newPos.z = testZ.z;

        return newPos;
    }

    void RunScene(GLFWwindow* window)
    {
        // Registrar callbacks locales para esta escena
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetScrollCallback(window, scroll_callback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // Reiniciar variables locales
        camera = Camera(glm::vec3(0.0f, 2.0f, 8.0f));
        firstMouse = true;
        g_inBlackout = false;
        g_nextBlackoutTime = -1.0f;
        g_blackoutEndTime = 0.0f;
        g_startleTriggerTime = -1000.0f;
        g_meshCenters.clear();
        g_autoDetectedLightIndices.clear();
        g_redLightPositions.clear();
        g_ornamentLightPositions.clear();
        g_ornamentLightColors.clear();
        g_redLightWasOff.clear();
        g_redLightWasPreStutter.clear();
        g_jumpscareTriggerTime = -1000.0f;
        g_jumpscareCooldownUntil = -1000.0f;
        g_jumpscareActive = false;
        g_jumpscareActiveEndTime = -1000.0f;
        g_zombieGazeTimer.clear();
        g_zombieCurrentXZ.clear();
        g_zombieMoveYaw.clear();
        g_pursuerIndex = -1;
        g_nextPresenceSoundTime = -1.0f;
        g_pursuerLungeTimer = 0.0f;
        g_pursuerRetreatUntil = -1000.0f;
        g_pursuerWasNear = false;
        exitPhase = EXIT_NONE;
        exitTimer = 0.0f;

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        SCR_WIDTH = width;
        SCR_HEIGHT = height;

        glEnable(GL_DEPTH_TEST);

        // Frame de cargando
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glfwSwapBuffers(window);
        glfwPollEvents();

        // Shaders
        Shader ourShader("shaders/model_loading.vs", "shaders/model_loading.fs");
        Shader bulbShader("shaders/light_billboard.vs", "shaders/light_billboard.fs");

        // Quad para bombillas
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

        // -------------------- Audio --------------------
        initaudio();
        preloadSound(SOUND_SCREAM_PATH.c_str(), "scream");
        preloadSound(SOUND_LIGHT_FX_PATH.c_str(), "lightfx");
        preloadSound(SOUND_CEILING_BLACKOUT_PATH.c_str(), "ceilingfail");
        preloadSound(SOUND_PRESENCE_PATH.c_str(), "presence");
        preloadSound(SOUND_PRESENCE_PATH.c_str(), "warning");
        playmusic(SOUND_AMBIENT_MUSIC_PATH.c_str());

        // -------------------- Jumpscare: shader, textura e imagen a pantalla completa --------------------
        Shader jumpscareShader("shaders/jumpscare.vs", "shaders/jumpscare.fs");
        unsigned int jumpscareTex = LoadTexture2D(JUMPSCARE_IMAGE_PATH);
        unsigned int mattMessageTex = LoadTexture2D(MATT_MESSAGE_IMAGE_PATH);

        float jumpscareQuad[] = {
            // pos          // uv
            -1.0f, -1.0f,   0.0f, 0.0f,
             1.0f, -1.0f,   1.0f, 0.0f,
             1.0f,  1.0f,   1.0f, 1.0f,
            -1.0f, -1.0f,   0.0f, 0.0f,
             1.0f,  1.0f,   1.0f, 1.0f,
            -1.0f,  1.0f,   0.0f, 1.0f,
        };
        unsigned int jumpscareVAO, jumpscareVBO;
        glGenVertexArrays(1, &jumpscareVAO);
        glGenBuffers(1, &jumpscareVBO);
        glBindVertexArray(jumpscareVAO);
        glBindBuffer(GL_ARRAY_BUFFER, jumpscareVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(jumpscareQuad), jumpscareQuad, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);

        // Textura blanca fallback
        unsigned int fallbackWhiteTex;
        glGenTextures(1, &fallbackWhiteTex);
        glBindTexture(GL_TEXTURE_2D, fallbackWhiteTex);
        unsigned char whitePixel[3] = { 255, 255, 255 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, whitePixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Carga de modelos (usando el nuevo archivo OBJ)
        Model ourModel("./model/Warehouse/abandoned_warehouse_-_interior_scene/interior_scene.obj");

        // AABB local
        glm::vec3 vmin(FLT_MAX), vmax(-FLT_MAX);
        for (auto& mesh : ourModel.meshes)
            for (auto& v : mesh.vertices) {
                vmin = glm::min(vmin, v.Position);
                vmax = glm::max(vmax, v.Position);
            }
        AABB localBox{ vmin, vmax };
        glm::vec3 localSize = localBox.max - localBox.min;

        float horizontalExtent = (std::max)(localSize.x, localSize.z);
        float autoScale = (horizontalExtent > 0.0001f) ? (TARGET_BUILDING_SIZE / horizontalExtent) : 1.0f;

        glm::mat4 warehouseModelMatrix = glm::mat4(1.0f);
        if (MODEL_IS_Z_UP)
            warehouseModelMatrix = glm::rotate(warehouseModelMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

        warehouseModelMatrix = glm::translate(warehouseModelMatrix, glm::vec3(0.0f, -localBox.min.y * autoScale, 0.0f));
        warehouseModelMatrix = glm::scale(warehouseModelMatrix, glm::vec3(autoScale));

        // AABB mundo
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

        BuildCollisionGrid(ourModel, warehouseModelMatrix, g_worldAABB);

        // -------------------- Modelo extra (arbol), centrado y a nivel de piso --------------------
        Model* extraModel = nullptr;
        glm::mat4 extraModelMatrix = glm::mat4(1.0f);
        // Punto de referencia (centro del arbol) para ubicar cosas alrededor
        // de el, como los modelos repetidos en X. Si no hay arbol, cae al
        // centro del edificio.
        glm::vec2 treeAnchorXZ((g_worldAABB.min.x + g_worldAABB.max.x) * 0.5f, (g_worldAABB.min.z + g_worldAABB.max.z) * 0.5f);
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

            // Punto "hacia el spawn" (sin buscar aun lugar libre, solo la
            // direccion): mezcla entre el centro del edificio y la esquina
            // de spawn configurada mas arriba (SPAWN_X_FRAC / SPAWN_Z_FRAC).
            glm::vec2 spawnApproxXZ(
                g_worldAABB.min.x + SPAWN_X_FRAC * (g_worldAABB.max.x - g_worldAABB.min.x),
                g_worldAABB.min.z + SPAWN_Z_FRAC * (g_worldAABB.max.z - g_worldAABB.min.z)
            );
            glm::vec2 targetXZ = glm::mix(buildingCenterXZ, spawnApproxXZ, EXTRA_MODEL_TOWARD_SPAWN_BLEND);

            // Radio horizontal aproximado del arbol ya escalado, para no
            // superponerlo con sillas/mesas ya marcadas como solidas.
            float treeRadius = 0.5f * std::max(eSize.x, eSize.z) * extraScale + EXTRA_MODEL_COLLISION_MARGIN;

            glm::vec2 finalXZ = targetXZ;
            glm::vec3 testPos(targetXZ.x, floorY + 0.1f, targetXZ.y);
            if (CollidesAt(testPos, treeRadius) || !HasFloorAt(testPos))
            {
                bool found = false;
                for (int r = 1; r <= 60 && !found; r++)
                {
                    float radius = r * (GRID_CELL_SIZE * 0.5f);
                    for (int a = 0; a < 16 && !found; a++)
                    {
                        float angle = (2.0f * 3.14159265f * a) / 16;
                        glm::vec2 candidate(targetXZ.x + cosf(angle) * radius, targetXZ.y + sinf(angle) * radius);
                        glm::vec3 candidatePos(candidate.x, floorY + 0.1f, candidate.y);
                        if (!CollidesAt(candidatePos, treeRadius) && HasFloorAt(candidatePos))
                        {
                            finalXZ = candidate;
                            found = true;
                        }
                    }
                }
                std::cout << "[DEBUG] Modelo extra reubicado para no chocar con muebles: (" << finalXZ.x << ", " << finalXZ.y << ")" << std::endl;
            }
            treeAnchorXZ = finalXZ;

            glm::vec3 worldPos(
                finalXZ.x - localCenterXZ.x * extraScale,
                floorY - emin.y * extraScale,
                finalXZ.y - localCenterXZ.y * extraScale
            );

            extraModelMatrix = glm::translate(glm::mat4(1.0f), worldPos);
            if (EXTRA_MODEL_IS_Z_UP)
                extraModelMatrix = glm::rotate(extraModelMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            extraModelMatrix = glm::scale(extraModelMatrix, glm::vec3(extraScale));

            std::cout << "[DEBUG] Modelo extra cargado. Escala=" << extraScale
                << " (altura local=" << eHeight << " -> objetivo=" << EXTRA_MODEL_TARGET_HEIGHT << ")" << std::endl;

            if (EXTRA_MODEL_ADD_COLLISION)
                AddModelCollision(*extraModel, extraModelMatrix, floorY);

            // -------------------- Bombillos de colores en el follaje --------------------
            if (EXTRA_MODEL_ORNAMENT_LIGHTS)
            {
                // Recorremos cada malla del arbol y calculamos su AABB de mundo,
                // para detectar cuales son "bombillos reales" (chicos y
                // compactos, tipo esfera) en vez de ramas/hojas/tronco.
                std::vector<glm::vec3> treeMeshCenters;
                std::vector<int> treeOrnamentCandidates;

                for (size_t m = 0; m < extraModel->meshes.size(); m++)
                {
                    auto& tverts = extraModel->meshes[m].vertices;
                    glm::vec3 mmin(FLT_MAX), mmax(-FLT_MAX);
                    for (auto& v : tverts) {
                        glm::vec3 wp = glm::vec3(extraModelMatrix * glm::vec4(v.Position, 1.0f));
                        mmin = glm::min(mmin, wp);
                        mmax = glm::max(mmax, wp);
                    }
                    glm::vec3 mCenter = (mmin + mmax) * 0.5f;
                    treeMeshCenters.push_back(mCenter);

                    float fx = mmax.x - mmin.x, fy = mmax.y - mmin.y, fz = mmax.z - mmin.z;
                    float footprint = std::max({ fx, fy, fz });
                    float minDim = std::max(std::min({ fx, fy, fz }), 0.001f);
                    float aspect = footprint / minDim;

                    bool isCandidate = footprint < ORNAMENT_MAX_FOOTPRINT && aspect < ORNAMENT_MAX_ASPECT_RATIO;
                    if (isCandidate)
                        treeOrnamentCandidates.push_back((int)m);

                    if (PRINT_MESH_DEBUG)
                        std::cout << "  [ARBOL MESH " << m << "] verts=" << tverts.size()
                        << " size=(" << fx << ", " << fy << ", " << fz << ")"
                        << (isCandidate ? "  -> CANDIDATO A BOMBILLO" : "") << std::endl;
                }

                g_ornamentLightPositions.clear();
                g_ornamentLightColors.clear();

                if (!manualOrnamentMeshIndices.empty())
                {
                    for (int idx : manualOrnamentMeshIndices)
                    {
                        if (idx >= 0 && idx < (int)treeMeshCenters.size())
                        {
                            g_ornamentLightPositions.push_back(treeMeshCenters[idx]);
                            int colorIdx = (int)g_ornamentLightPositions.size() % ORNAMENT_PALETTE_SIZE;
                            g_ornamentLightColors.push_back(ORNAMENT_PALETTE[colorIdx]);
                        }
                    }
                    std::cout << "[DEBUG] Usando " << g_ornamentLightPositions.size() << " bombillo(s) definidos manualmente en manualOrnamentMeshIndices." << std::endl;
                }
                else if (!treeOrnamentCandidates.empty())
                {
                    for (int idx : treeOrnamentCandidates)
                    {
                        if ((int)g_ornamentLightPositions.size() >= ORNAMENT_LIGHT_COUNT) break;
                        g_ornamentLightPositions.push_back(treeMeshCenters[idx]);
                        int colorIdx = (int)g_ornamentLightPositions.size() % ORNAMENT_PALETTE_SIZE;
                        g_ornamentLightColors.push_back(ORNAMENT_PALETTE[colorIdx]);
                    }
                    std::cout << "[DEBUG] " << treeOrnamentCandidates.size() << " candidato(s) a bombillo detectados automaticamente, usando "
                        << g_ornamentLightPositions.size() << "." << std::endl;
                }
                else
                {
                    // Respaldo: si no se detecto ningun bombillo real, repartimos
                    // puntos dentro del volumen del arbol (mejor que nada, pero
                    // pueden caer en huecos entre ramas -- avisa por consola).
                    glm::vec3 tmin(FLT_MAX), tmax(-FLT_MAX);
                    glm::vec3 corners[8] = {
                        {emin.x, emin.y, emin.z}, {emax.x, emin.y, emin.z},
                        {emin.x, emax.y, emin.z}, {emin.x, emin.y, emax.z},
                        {emax.x, emax.y, emin.z}, {emax.x, emin.y, emax.z},
                        {emin.x, emax.y, emax.z}, {emax.x, emax.y, emax.z},
                    };
                    for (auto& c : corners) {
                        glm::vec3 wc = glm::vec3(extraModelMatrix * glm::vec4(c, 1.0f));
                        tmin = glm::min(tmin, wc);
                        tmax = glm::max(tmax, wc);
                    }
                    glm::vec3 treeCenter = (tmin + tmax) * 0.5f;
                    float halfWidth = (tmax.x - tmin.x) * 0.5f;
                    float halfDepth = (tmax.z - tmin.z) * 0.5f;

                    for (int i = 0; i < ORNAMENT_LIGHT_COUNT; i++)
                    {
                        float heightFrac = 0.45f + PseudoRandom01((float)i * 13.7f + 1.0f) * 0.5f;
                        float y = tmin.y + heightFrac * (tmax.y - tmin.y);
                        float angle = PseudoRandom01((float)i * 29.3f + 2.0f) * 6.2831853f;
                        float radiusFrac = 0.15f + PseudoRandom01((float)i * 41.1f + 3.0f) * 0.4f;
                        float x = treeCenter.x + cosf(angle) * radiusFrac * halfWidth;
                        float z = treeCenter.z + sinf(angle) * radiusFrac * halfDepth;

                        g_ornamentLightPositions.push_back(glm::vec3(x, y, z));
                        int colorIdx = (int)(PseudoRandom01((float)i * 53.7f + 4.0f) * ORNAMENT_PALETTE_SIZE) % ORNAMENT_PALETTE_SIZE;
                        g_ornamentLightColors.push_back(ORNAMENT_PALETTE[colorIdx]);
                    }
                    std::cout << "[DEBUG] No se detecto ningun bombillo real: usando reparto aleatorio (revisar manualOrnamentMeshIndices si no queda bien)." << std::endl;
                }

                std::cout << "[DEBUG] Total bombillos activos: " << g_ornamentLightPositions.size() << std::endl;
            }
        }

        // Cargamos modelos repetidos (zombies)
        struct RepeatedInstance {
            glm::vec3 worldPos;
            float yawDeg;
            float scale;
        };
        struct ZombiePart {
            int meshIndex;
            int type;
            glm::vec3 pivotLocal;
        };
        Model* repeatedModel = nullptr;
        std::vector<RepeatedInstance> repeatedInstances;
        std::vector<ZombiePart> zombieParts;
        // Pivote local (centro horizontal + base) y radio de colision del
        // modelo repetido, calculados una vez y reusados tanto al ubicarlo
        // como en la persecucion del render loop (rotar/mover alrededor del
        // pivote real evita el desfase que hacia que se metieran en columnas).
        glm::vec3 repeatedPivotLocal(0.0f);
        float repeatedFootprintRadius = 0.5f;

        if (ADD_REPEATED_MODEL)
        {
            repeatedModel = new Model(REPEATED_MODEL_PATH);

            glm::vec3 rmin(FLT_MAX), rmax(-FLT_MAX);
            for (auto& mesh : repeatedModel->meshes)
                for (auto& v : mesh.vertices) {
                    rmin = glm::min(rmin, v.Position);
                    rmax = glm::max(rmax, v.Position);
                }
            glm::vec3 rSize = rmax - rmin;
            float rHeight = rSize.y;
            float repeatedScale = (rHeight > 0.0001f) ? (REPEATED_MODEL_TARGET_HEIGHT / rHeight) : 1.0f;
            glm::vec2 rLocalCenterXZ((rmin.x + rmax.x) * 0.5f, (rmin.z + rmax.z) * 0.5f);
            float rFootprintRadius = 0.5f * (std::max)(rSize.x, rSize.z) * repeatedScale + REPEATED_MODEL_COLLISION_MARGIN;
            float floorY = g_worldAABB.min.y;

            repeatedPivotLocal = glm::vec3(rLocalCenterXZ.x, rmin.y, rLocalCenterXZ.y);
            repeatedFootprintRadius = rFootprintRadius;

            if (ZOMBIE_ANIMATE_PARTS)
            {
                glm::vec3 overallCenter = (rmin + rmax) * 0.5f;
                float halfWidth = (rmax.x - rmin.x) * 0.5f;
                float totalHeight = rmax.y - rmin.y;

                for (size_t m = 0; m < repeatedModel->meshes.size(); m++)
                {
                    auto& verts = repeatedModel->meshes[m].vertices;
                    glm::vec3 mmin(FLT_MAX), mmax(-FLT_MAX);
                    for (auto& v : verts) {
                        mmin = glm::min(mmin, v.Position);
                        mmax = glm::max(mmax, v.Position);
                    }
                    glm::vec3 mCenter = (mmin + mmax) * 0.5f;
                    float heightFrac = (totalHeight > 0.0001f) ? (mCenter.y - rmin.y) / totalHeight : 0.0f;
                    float offsetXFrac = (halfWidth > 0.0001f) ? (mCenter.x - overallCenter.x) / halfWidth : 0.0f;

                    bool isHeadAuto = heightFrac > ZOMBIE_HEAD_HEIGHT_FRAC_MIN && fabsf(offsetXFrac) < ZOMBIE_HEAD_MAX_OFFSET_FRAC;
                    bool isArmAuto = heightFrac > ZOMBIE_ARM_HEIGHT_FRAC_MIN && heightFrac < ZOMBIE_ARM_HEIGHT_FRAC_MAX && fabsf(offsetXFrac) > ZOMBIE_ARM_MIN_OFFSET_FRAC;

                    bool manualHead = std::find(manualZombieHeadMeshIndices.begin(), manualZombieHeadMeshIndices.end(), (int)m) != manualZombieHeadMeshIndices.end();
                    bool manualArm = std::find(manualZombieArmMeshIndices.begin(), manualZombieArmMeshIndices.end(), (int)m) != manualZombieArmMeshIndices.end();

                    bool useAutoHead = manualZombieHeadMeshIndices.empty() && isHeadAuto;
                    bool useAutoArm = manualZombieArmMeshIndices.empty() && isArmAuto;

                    int type = 0;
                    glm::vec3 pivot = mCenter;
                    if (manualHead || useAutoHead)
                    {
                        type = 1;
                        pivot = glm::vec3(mCenter.x, mmin.y, mCenter.z);
                    }
                    else if (manualArm || useAutoArm)
                    {
                        type = 2;
                        pivot = glm::vec3(mCenter.x, mmax.y, mCenter.z);
                    }

                    zombieParts.push_back({ (int)m, type, pivot });
                }
            }

            for (int i = 0; i < REPEATED_MODEL_COUNT; i++)
            {
                float angle = (REPEATED_MODEL_COUNT == 4)
                    ? glm::radians(45.0f + 90.0f * i)
                    : (2.0f * 3.14159265f * i) / REPEATED_MODEL_COUNT;

                glm::vec2 targetXZ(
                    treeAnchorXZ.x + cosf(angle) * REPEATED_MODEL_RADIUS_FROM_TREE,
                    treeAnchorXZ.y + sinf(angle) * REPEATED_MODEL_RADIUS_FROM_TREE
                );

                glm::vec2 finalRXZ = targetXZ;
                glm::vec3 testPos(targetXZ.x, floorY + 0.1f, targetXZ.y);
                if (CollidesAt(testPos, rFootprintRadius) || !HasFloorAt(testPos))
                {
                    bool found = false;
                    for (int r2 = 1; r2 <= 60 && !found; r2++)
                    {
                        float radius = r2 * (GRID_CELL_SIZE * 0.5f);
                        for (int a = 0; a < 16 && !found; a++)
                        {
                            float a2 = (2.0f * 3.14159265f * a) / 16;
                            glm::vec2 candidate(targetXZ.x + cosf(a2) * radius, targetXZ.y + sinf(a2) * radius);
                            glm::vec3 candidatePos(candidate.x, floorY + 0.1f, candidate.y);
                            if (!CollidesAt(candidatePos, rFootprintRadius) && HasFloorAt(candidatePos))
                            {
                                finalRXZ = candidate;
                                found = true;
                            }
                        }
                    }
                }

                // Punto de mundo donde debe terminar el pivote (centro
                // horizontal + base) de esta instancia, sin importar el angulo.
                glm::vec3 worldPivotTarget(finalRXZ.x, floorY, finalRXZ.y);

                float yawDeg = 0.0f;
                if (REPEATED_MODEL_FACE_CENTER)
                {
                    glm::vec2 toCenter = glm::normalize(treeAnchorXZ - finalRXZ);
                    yawDeg = glm::degrees(atan2f(toCenter.x, toCenter.y)) + REPEATED_MODEL_YAW_OFFSET_DEG;
                }

                // La rotacion se hace alrededor del PIVOTE del modelo (centro
                // horizontal + base), no del origen local -- si no, al girar
                // cada instancia para mirar al arbol, el cuerpo se corre de
                // la posicion validada como libre y puede meterse en una
                // columna que la busqueda si habia esquivado.
                glm::mat4 baseMatrix = glm::translate(glm::mat4(1.0f), worldPivotTarget);
                baseMatrix = glm::rotate(baseMatrix, glm::radians(yawDeg), glm::vec3(0.0f, 1.0f, 0.0f));
                if (REPEATED_MODEL_IS_Z_UP)
                    baseMatrix = glm::rotate(baseMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                baseMatrix = glm::scale(baseMatrix, glm::vec3(repeatedScale));
                baseMatrix = glm::translate(baseMatrix, -repeatedPivotLocal);

                // Ver nota de diseno arriba: no se agrega colision fija
                // (REPEATED_MODEL_ADD_COLLISION queda como interruptor por si
                // en algun momento se desactiva la persecucion y se prefiere
                // volver a colision solida clasica).
                if (REPEATED_MODEL_ADD_COLLISION && !ZOMBIE_APPROACH_ENABLED)
                    AddModelCollision(*repeatedModel, baseMatrix, floorY);

                repeatedInstances.push_back({ worldPivotTarget, yawDeg, repeatedScale });
            }
        }

        // Spawn cámara
        glm::vec3 spawnTarget(
            g_worldAABB.min.x + SPAWN_X_FRAC * (g_worldAABB.max.x - g_worldAABB.min.x),
            g_worldAABB.min.y + PLAYER_EYE_HEIGHT,
            g_worldAABB.min.z + SPAWN_Z_FRAC * (g_worldAABB.max.z - g_worldAABB.min.z)
        );
        camera.Position = spawnTarget;
        camera.MovementSpeed = PLAYER_WALK_SPEED;

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

        // Orientar cámara hacia el centro del edificio
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

                glm::vec3 newFront;
                newFront.x = cosf(glm::radians(camera.Yaw)) * cosf(glm::radians(camera.Pitch));
                newFront.y = sinf(glm::radians(camera.Pitch));
                newFront.z = sinf(glm::radians(camera.Yaw)) * cosf(glm::radians(camera.Pitch));
                camera.Front = glm::normalize(newFront);
                camera.Right = glm::normalize(glm::cross(camera.Front, camera.WorldUp));
                camera.Up = glm::normalize(glm::cross(camera.Right, camera.Front));
            }
        }

        lastFrame = static_cast<float>(glfwGetTime());

        // Bucle render
        while (!glfwWindowShouldClose(window) && g_NextScene == g_CurrentScene)
        {
            float currentFrame = static_cast<float>(glfwGetTime());
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            if (deltaTime > 0.1f) deltaTime = 0.1f;

            processInput(window);

            // Si estamos en la secuencia de salida, saltamos la lógica y renderizado normal 3D
            if (exitPhase != EXIT_NONE)
            {
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                glDisable(GL_DEPTH_TEST);
                jumpscareShader.use();
                jumpscareShader.setInt("jumpscareTex", 0);

                float fadeValue = 0.0f;
                float fadeDuration = 2.5f;

                if (exitPhase == EXIT_WAIT_BLACK)
                {
                    if (currentFrame - exitTimer >= 2.0f) { // 2 Segundos en negro
                        exitPhase = EXIT_FADE_IN;
                        exitTimer = currentFrame;
                    }
                }
                else if (exitPhase == EXIT_FADE_IN)
                {
                    float progress = (currentFrame - exitTimer) / fadeDuration;
                    fadeValue = glm::clamp(progress, 0.0f, 1.0f);
                    if (progress >= 1.0f) {
                        exitPhase = EXIT_SHOW;
                        exitTimer = currentFrame;
                    }
                }
                else if (exitPhase == EXIT_SHOW)
                {
                    fadeValue = 1.0f;
                    if (currentFrame - exitTimer >= 10.0f) { // Mostrar 10 segundos
                        exitPhase = EXIT_FADE_OUT;
                        exitTimer = currentFrame;
                    }
                }
                else if (exitPhase == EXIT_FADE_OUT)
                {
                    float progress = (currentFrame - exitTimer) / fadeDuration;
                    fadeValue = 1.0f - glm::clamp(progress, 0.0f, 1.0f);
                    if (progress >= 1.0f) {
                        if (g_UnlockedLevel < 4) {
                            g_UnlockedLevel = 4;
                        }
                        g_NextScene = SCENE_PASILLO; // Regresar al Pasillo
                    }
                }

                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                jumpscareShader.setFloat("alpha", fadeValue);
                jumpscareShader.setFloat("blackOverride", 0.0f);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, mattMessageTex);

                glBindVertexArray(jumpscareVAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);

                glDisable(GL_BLEND);

                glfwSwapBuffers(window);
                glfwPollEvents();
                continue; // Saltar el resto del bucle normal de renderizado
            }

            // Reseteo del estado activo del jumpscare cuando termina su ventana de efecto
            if (g_jumpscareActive && currentFrame >= g_jumpscareActiveEndTime)
                g_jumpscareActive = false;

            glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            ourShader.use();

            // Actualizar tamaño de ventana
            glfwGetFramebufferSize(window, &width, &height);
            SCR_WIDTH = width;
            SCR_HEIGHT = height;

            // Golpe de zoom durante el jumpscare: el FOV salta de golpe y se
            // relaja hasta volver a lo normal en JUMPSCARE_EFFECT_DURATION.
            float jumpscareZoomPunch = 0.0f;
            if (g_jumpscareActive)
            {
                float progress = glm::clamp((g_jumpscareActiveEndTime - currentFrame) / JUMPSCARE_EFFECT_DURATION, 0.0f, 1.0f);
                jumpscareZoomPunch = JUMPSCARE_ZOOM_PUNCH * progress;
            }

            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom + jumpscareZoomPunch),
                (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 500.0f);
            // Sacudida de camara durante el jumpscare: mucho mas contundente
            // que un simple flash. Se aplica como offset a la posicion usada
            // SOLO para la vista de este frame, no afecta camera.Position real.
            glm::mat4 view;
            if (g_jumpscareActive)
            {
                float shakeProgress = glm::clamp((g_jumpscareActiveEndTime - currentFrame) / JUMPSCARE_EFFECT_DURATION, 0.0f, 1.0f);
                float shakeMag = JUMPSCARE_SHAKE_AMOUNT * shakeProgress;
                glm::vec3 shakeOffset(
                    (PseudoRandom01(currentFrame * 97.3f) - 0.5f) * 2.0f,
                    (PseudoRandom01(currentFrame * 131.7f + 11.0f) - 0.5f) * 2.0f,
                    (PseudoRandom01(currentFrame * 61.9f + 23.0f) - 0.5f) * 2.0f
                );
                shakeOffset *= shakeMag;
                view = glm::lookAt(camera.Position + shakeOffset, camera.Position + shakeOffset + camera.Front, camera.Up);
            }
            else
            {
                view = camera.GetViewMatrix();
            }
            ourShader.setMat4("projection", projection);
            ourShader.setMat4("view", view);
            ourShader.setMat4("model", warehouseModelMatrix);

            // Iluminación
            ourShader.setVec3("viewPos", camera.Position);
            ourShader.setVec3("flashlightDir", camera.Front);
            ourShader.setFloat("flashlightCutOff", cosf(glm::radians(FLASHLIGHT_INNER_CUTOFF_DEG)));
            ourShader.setFloat("flashlightOuterCutOff", cosf(glm::radians(FLASHLIGHT_OUTER_CUTOFF_DEG)));

            // Apagon linterna
            if (g_nextBlackoutTime < 0.0f)
                g_nextBlackoutTime = currentFrame + PseudoRandomRange(1.0f, BLACKOUT_MIN_INTERVAL, BLACKOUT_MAX_INTERVAL);

            if (!g_inBlackout && currentFrame >= g_nextBlackoutTime)
            {
                g_inBlackout = true;
                g_blackoutEndTime = currentFrame + PseudoRandomRange(currentFrame, BLACKOUT_MIN_DURATION, BLACKOUT_MAX_DURATION);
                g_startleTriggerTime = currentFrame;
                playPreloaded("lightfx");
            }
            else if (g_inBlackout && currentFrame >= g_blackoutEndTime)
            {
                g_inBlackout = false;
                g_nextBlackoutTime = currentFrame + PseudoRandomRange(currentFrame * 1.37f, BLACKOUT_MIN_INTERVAL, BLACKOUT_MAX_INTERVAL);
                playPreloaded("lightfx");
            }

            float flashlightFlicker;
            float flickerStep = floorf(currentFrame / FLICKER_STEP_DURATION);
            float flickerRoll = PseudoRandom01(flickerStep);
            if (flickerRoll < FLICKER_DIP_CHANCE)
            {
                float dipAmount = PseudoRandom01(flickerStep * 7.31f);
                flashlightFlicker = FLICKER_DIP_MIN + dipAmount * 0.15f;
            }
            else
            {
                flashlightFlicker = 0.8f + PseudoRandom01(flickerStep * 3.17f) * 0.2f;
            }

            if (g_inBlackout)
                flashlightFlicker = 0.0f;

            // El jumpscare pisa el parpadeo normal con un estrobo violento
            if (g_jumpscareActive)
                flashlightFlicker = (fmodf(currentFrame * 40.0f, 1.0f) > 0.5f) ? 1.5f : 0.0f;

            ourShader.setFloat("flashlightFlicker", flashlightFlicker);

            // Luces rojas
            const float GOLDEN_ANGLE = 2.399963f;
            if (g_redLightWasOff.size() != g_redLightPositions.size())
                g_redLightWasOff.assign(g_redLightPositions.size(), false);
            if (g_redLightWasPreStutter.size() != g_redLightPositions.size())
                g_redLightWasPreStutter.assign(g_redLightPositions.size(), false);

            std::vector<float> perLightIntensity(g_redLightPositions.size());
            for (size_t i = 0; i < g_redLightPositions.size(); i++)
            {
                float phase = (float)i * GOLDEN_ANGLE;
                float waveSlow = 0.5f + 0.5f * sinf(currentFrame * RED_LIGHT_PULSE_SPEED + phase);
                float waveFast = 0.5f + 0.5f * sinf(currentFrame * RED_LIGHT_PULSE_SPEED * 5.3f + phase * 1.7f);
                float combinedWave = waveSlow * 0.8f + waveFast * 0.2f;
                float value = RED_LIGHT_BASE_INTENSITY + RED_LIGHT_PULSE_AMOUNT * combinedWave;

                float period = PseudoRandomRange((float)i * 12.11f + 1.0f, RED_LIGHT_OFF_PERIOD_MIN, RED_LIGHT_OFF_PERIOD_MAX);
                float offDuration = PseudoRandomRange((float)i * 27.53f + 2.0f, RED_LIGHT_OFF_DURATION_MIN, RED_LIGHT_OFF_DURATION_MAX);
                float phaseOffset = PseudoRandomRange((float)i * 41.77f + 3.0f, 0.0f, period);
                float cyclePos = fmodf(currentFrame + phaseOffset, period);

                const float PRE_OFF_STUTTER = 1.4f;
                const float POST_ON_STUTTER = 1.1f;
                const float STUTTER_RATE = 13.0f;

                bool isOff = cyclePos < offDuration;
                bool isPreOffStutter = cyclePos >= (period - PRE_OFF_STUTTER);
                bool isPostOnStutter = !isOff && cyclePos < (offDuration + POST_ON_STUTTER);

                if (isPreOffStutter != g_redLightWasPreStutter[i])
                {
                    g_redLightWasPreStutter[i] = isPreOffStutter;
                    if (isPreOffStutter)
                        playPreloaded("ceilingfail"); // arranca el crujido justo cuando empieza a fallar, no cuando ya esta oscura
                }

                if (isPreOffStutter || isPostOnStutter)
                {
                    float stutterStep = floorf(currentFrame * STUTTER_RATE + (float)i * 3.7f);
                    float stutterRoll = PseudoRandom01(stutterStep * 7.91f + (float)i * 101.3f);
                    value = (stutterRoll > 0.45f) ? value : value * 0.05f;
                }

                if (isOff != g_redLightWasOff[i])
                {
                    g_redLightWasOff[i] = isOff;
                }
                if (isOff)
                    value = 0.0f;

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

            // Bombillos de colores del arbol: parpadeo tipo "luces de navidad"
            // (bloques de brillo que cambian cada tanto, no un fundido suave),
            // cada uno con su propio desfase para que no titilen sincronizados.
            std::vector<float> ornamentIntensity(g_ornamentLightPositions.size());
            for (size_t i = 0; i < g_ornamentLightPositions.size(); i++)
            {
                float step = floorf(currentFrame * 2.2f + (float)i * 1.37f);
                float roll = PseudoRandom01(step * 3.71f + (float)i * 57.3f);
                float twinkle = 0.25f + 0.75f * roll;
                ornamentIntensity[i] = ORNAMENT_LIGHT_BASE_INTENSITY * twinkle;
            }
            ourShader.setInt("numOrnamentLights", (int)g_ornamentLightPositions.size());
            for (size_t i = 0; i < g_ornamentLightPositions.size(); i++)
            {
                ourShader.setVec3("ornamentLightPositions[" + std::to_string(i) + "]", g_ornamentLightPositions[i]);
                ourShader.setVec3("ornamentLightColors[" + std::to_string(i) + "]", g_ornamentLightColors[i]);
                ourShader.setFloat("ornamentLightIntensities[" + std::to_string(i) + "]", ornamentIntensity[i]);
            }

            // Shaders parametros extras
            ourShader.setVec3("fogColor", FOG_COLOR);
            ourShader.setFloat("fogDensity", FOG_DENSITY);
            ourShader.setVec2("screenSize", glm::vec2((float)SCR_WIDTH, (float)SCR_HEIGHT));
            ourShader.setFloat("desaturation", DESATURATION_AMOUNT);
            ourShader.setFloat("materialShininess", MATERIAL_SHININESS);
            ourShader.setFloat("uTime", currentFrame);
            ourShader.setFloat("contrastPower", CONTRAST_POWER);
            ourShader.setFloat("grainAmount", GRAIN_AMOUNT);

            // Dread continuo: 0 si no hay perseguidor activo o esta lejos,
            // sube cuanto mas cerca este (usa el mismo radio que el aviso
            // de cercania para que quede coherente con el resto).
            float dread = 0.0f;
            if (g_pursuerIndex >= 0 && g_pursuerIndex < (int)g_zombieCurrentXZ.size())
            {
                float d = glm::length(glm::vec2(camera.Position.x, camera.Position.z) - g_zombieCurrentXZ[g_pursuerIndex]);
                dread = glm::clamp(1.0f - d / (ZOMBIE_WARNING_RANGE * 2.0f), 0.0f, 1.0f);
            }
            ourShader.setFloat("dread", dread);

            float startleElapsed = currentFrame - g_startleTriggerTime;
            float startleFlash = (startleElapsed >= 0.0f && startleElapsed < STARTLE_FLASH_DURATION)
                ? (1.0f - startleElapsed / STARTLE_FLASH_DURATION)
                : 0.0f;
            ourShader.setFloat("startleFlash", startleFlash);

            // Dibujar warehouse
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, fallbackWhiteTex);
            ourModel.Draw(ourShader);

            // Dibujar extraModel (árbol)
            if (extraModel)
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, fallbackWhiteTex);
                ourShader.setMat4("model", extraModelMatrix);
                extraModel->Draw(ourShader);
            }

            // -------- Inicializacion perezosa de estado dinamico por zombie --------
            if (g_zombieGazeTimer.size() != repeatedInstances.size())
                g_zombieGazeTimer.assign(repeatedInstances.size(), 0.0f);
            if (g_zombieCurrentXZ.size() != repeatedInstances.size())
            {
                g_zombieCurrentXZ.resize(repeatedInstances.size());
                g_zombieMoveYaw.resize(repeatedInstances.size());
                for (size_t k = 0; k < repeatedInstances.size(); k++)
                {
                    g_zombieCurrentXZ[k] = glm::vec2(repeatedInstances[k].worldPos.x, repeatedInstances[k].worldPos.z);
                    g_zombieMoveYaw[k] = repeatedInstances[k].yawDeg;
                }
            }

            // -------- Persecucion "no me mires": solo el mas cercano, solo tras pasar el arbol --------
            if (ZOMBIE_APPROACH_ENABLED && !repeatedInstances.empty())
            {
                glm::vec2 spawnXZ(
                    g_worldAABB.min.x + SPAWN_X_FRAC * (g_worldAABB.max.x - g_worldAABB.min.x),
                    g_worldAABB.min.z + SPAWN_Z_FRAC * (g_worldAABB.max.z - g_worldAABB.min.z)
                );
                float playerProgress = glm::length(glm::vec2(camera.Position.x, camera.Position.z) - spawnXZ);
                float treeProgress = glm::length(treeAnchorXZ - spawnXZ);
                bool passedTree = playerProgress > treeProgress;

                if (passedTree)
                {
                    // Se elige el perseguidor UNA sola vez (el mas cercano en
                    // ese instante) y se mantiene el mismo de ahi en adelante
                    // -- asi el avance no se reparte entre los 4 y se siente
                    // como que ALGUIEN especifico te viene siguiendo.
                    if (g_pursuerIndex < 0 || g_pursuerIndex >= (int)repeatedInstances.size())
                    {
                        size_t nearestIdx = 0;
                        float nearestDist = FLT_MAX;
                        for (size_t k = 0; k < repeatedInstances.size(); k++)
                        {
                            float d = glm::length(glm::vec2(camera.Position.x, camera.Position.z) - g_zombieCurrentXZ[k]);
                            if (d < nearestDist) { nearestDist = d; nearestIdx = k; }
                        }
                        g_pursuerIndex = (int)nearestIdx;
                        std::cout << "[DEBUG] Zombie " << g_pursuerIndex << " elegido como perseguidor fijo." << std::endl;
                    }
                    size_t nearestIdx = (size_t)g_pursuerIndex;

                    bool inGracePeriod = currentFrame < g_pursuerRetreatUntil;

                    glm::vec3 approachPos3D(g_zombieCurrentXZ[nearestIdx].x, g_worldAABB.min.y + 0.1f, g_zombieCurrentXZ[nearestIdx].y);
                    glm::vec3 toZ = approachPos3D - camera.Position;
                    float distToZ = glm::length(toZ);
                    bool looking = false;
                    if (distToZ > 0.001f && distToZ < ZOMBIE_APPROACH_LOOK_MAX_DISTANCE)
                    {
                        glm::vec3 dirToZ = toZ / distToZ;
                        float cosAngle = glm::dot(dirToZ, glm::normalize(camera.Front));
                        looking = cosAngle > cosf(glm::radians(ZOMBIE_APPROACH_LOOK_ANGLE_DEG));
                    }

                    if (!inGracePeriod && !looking)
                    {
                        // Se acerca hasta guardar cierta distancia (ya no se
                        // pega a la camara como antes).
                        glm::vec2 toPlayerXZ = glm::vec2(camera.Position.x, camera.Position.z) - g_zombieCurrentXZ[nearestIdx];
                        float distXZ = glm::length(toPlayerXZ);
                        if (distXZ > ZOMBIE_APPROACH_STOP_DISTANCE)
                        {
                            glm::vec2 dir = toPlayerXZ / distXZ;
                            glm::vec2 step = dir * ZOMBIE_APPROACH_SPEED * deltaTime;

                            glm::vec3 testX(g_zombieCurrentXZ[nearestIdx].x + step.x, g_worldAABB.min.y + 0.1f, g_zombieCurrentXZ[nearestIdx].y);
                            if (!CollidesAt(testX, repeatedFootprintRadius) && HasFloorAt(testX))
                                g_zombieCurrentXZ[nearestIdx].x += step.x;

                            glm::vec3 testZ(g_zombieCurrentXZ[nearestIdx].x, g_worldAABB.min.y + 0.1f, g_zombieCurrentXZ[nearestIdx].y + step.y);
                            if (!CollidesAt(testZ, repeatedFootprintRadius) && HasFloorAt(testZ))
                                g_zombieCurrentXZ[nearestIdx].y += step.y;

                            g_zombieMoveYaw[nearestIdx] = glm::degrees(atan2f(dir.x, dir.y));

                            // Limite de "territorio": si el paso lo saco mas
                            // alla de su rango maximo desde su spawn original,
                            // lo dejamos justo en el borde -- se queda ahi
                            // quieto en vez de perseguir por todo el warehouse.
                            glm::vec2 pursuerSpawnXZ(repeatedInstances[nearestIdx].worldPos.x, repeatedInstances[nearestIdx].worldPos.z);
                            float distFromSpawn = glm::length(g_zombieCurrentXZ[nearestIdx] - pursuerSpawnXZ);
                            if (distFromSpawn > ZOMBIE_APPROACH_MAX_RANGE)
                            {
                                glm::vec2 dirFromSpawn = (g_zombieCurrentXZ[nearestIdx] - pursuerSpawnXZ) / distFromSpawn;
                                g_zombieCurrentXZ[nearestIdx] = pursuerSpawnXZ + dirFromSpawn * ZOMBIE_APPROACH_MAX_RANGE;
                            }
                        }

                        // Aviso de cercania: suena UNA vez al cruzar de lejos
                        // a cerca (no se repite mientras se mantiene cerca).
                        bool isNearNow = distXZ < ZOMBIE_WARNING_RANGE;
                        if (isNearNow && !g_pursuerWasNear)
                            playPreloaded("warning");
                        g_pursuerWasNear = isNearNow;

                        // Embestida: si te quedas sin mirarlo mientras esta
                        // relativamente cerca por varios segundos seguidos,
                        // cierra la distancia de golpe y ataca.
                        if (distXZ < ZOMBIE_LUNGE_ENGAGE_RANGE)
                            g_pursuerLungeTimer += deltaTime;
                        else
                            g_pursuerLungeTimer = (std::max)(0.0f, g_pursuerLungeTimer - deltaTime);

                        bool lunging = g_pursuerLungeTimer >= ZOMBIE_LUNGE_THRESHOLD;
                        bool directContact = distXZ < ZOMBIE_APPROACH_ATTACK_DISTANCE;

                        if ((lunging || directContact) &&
                            currentFrame >= g_jumpscareCooldownUntil && !g_jumpscareActive)
                        {
                            if (lunging)
                            {
                                // Cierra el resto de la distancia de un salto
                                // (validando que no quede metido en una pared).
                                glm::vec2 lungeDir = (distXZ > 0.001f) ? (toPlayerXZ / distXZ) : glm::vec2(1.0f, 0.0f);
                                glm::vec2 lungeTarget = glm::vec2(camera.Position.x, camera.Position.z) - lungeDir * (ZOMBIE_APPROACH_ATTACK_DISTANCE * 0.8f);
                                glm::vec3 lungeTest(lungeTarget.x, g_worldAABB.min.y + 0.1f, lungeTarget.y);
                                if (!CollidesAt(lungeTest, repeatedFootprintRadius) && HasFloorAt(lungeTest))
                                    g_zombieCurrentXZ[nearestIdx] = lungeTarget;
                            }

                            g_jumpscareTriggerTime = currentFrame;
                            g_jumpscareActive = true;
                            g_jumpscareActiveEndTime = currentFrame + JUMPSCARE_EFFECT_DURATION;
                            g_jumpscareCooldownUntil = currentFrame + JUMPSCARE_COOLDOWN;
                            g_startleTriggerTime = currentFrame;
                            playPreloaded("scream");
                            std::cout << "[DEBUG] *** JUMPSCARE *** Zombie " << nearestIdx << " te alcanzo." << std::endl;

                            // Retirada: vuelve a su posicion original (cerca
                            // del arbol) y espera antes de volver a acercarse.
                            // Ademas se resetea el indice de perseguidor: la
                            // PROXIMA vez se re-evalua cual es el mas cercano
                            // (puede ser el mismo u otro distinto).
                            g_zombieCurrentXZ[nearestIdx] = glm::vec2(repeatedInstances[nearestIdx].worldPos.x, repeatedInstances[nearestIdx].worldPos.z);
                            g_pursuerRetreatUntil = currentFrame + ZOMBIE_RETREAT_GRACE_DURATION;
                            g_pursuerLungeTimer = 0.0f;
                            g_pursuerWasNear = false;
                            g_pursuerIndex = -1;
                            std::cout << "[DEBUG] Zombie " << nearestIdx << " se retira, vuelve a acercarse en " << ZOMBIE_RETREAT_GRACE_DURATION << "s." << std::endl;
                        }
                    }
                    else if (looking)
                    {
                        // Si lo estas mirando, se congela Y se calma (pierde
                        // urgencia de embestir) mas rapido que si solo te
                        // alejaras sin mirarlo.
                        g_pursuerLungeTimer = (std::max)(0.0f, g_pursuerLungeTimer - deltaTime * 2.0f);
                    }
                }
            }

            // Sonido de "alguien te sigue": suena de vez en cuando SOLO
            // mientras hay un perseguidor activo, en un intervalo aleatorio.
            if (g_pursuerIndex >= 0)
            {
                if (g_nextPresenceSoundTime < 0.0f)
                    g_nextPresenceSoundTime = currentFrame + PseudoRandomRange(1.0f, PRESENCE_SOUND_MIN_INTERVAL, PRESENCE_SOUND_MAX_INTERVAL);
                if (currentFrame >= g_nextPresenceSoundTime)
                {
                    playPreloaded("presence");
                    g_nextPresenceSoundTime = currentFrame + PseudoRandomRange(currentFrame, PRESENCE_SOUND_MIN_INTERVAL, PRESENCE_SOUND_MAX_INTERVAL);
                }
            }

            // Dibujar zombies
            if (repeatedModel)
            {
                for (size_t i = 0; i < repeatedInstances.size(); i++)
                {
                    auto& inst = repeatedInstances[i];

                    // -------- Jumpscare por mirada sostenida (aplica a cualquiera) --------
                    if (JUMPSCARE_STARE_ENABLED)
                    {
                        glm::vec3 gazeTarget(g_zombieCurrentXZ[i].x, inst.worldPos.y + REPEATED_MODEL_TARGET_HEIGHT * 0.6f, g_zombieCurrentXZ[i].y);
                        glm::vec3 toZombie = gazeTarget - camera.Position;
                        float distToGaze = glm::length(toZombie);
                        bool staring = false;
                        if (distToGaze > 0.001f && distToGaze < JUMPSCARE_STARE_MAX_DISTANCE)
                        {
                            glm::vec3 dirToZombie = toZombie / distToGaze;
                            float angleCos = glm::dot(glm::normalize(camera.Front), dirToZombie);
                            staring = angleCos > cosf(glm::radians(JUMPSCARE_STARE_FOV_ANGLE_DEG));
                        }

                        if (staring)
                            g_zombieGazeTimer[i] += deltaTime;
                        else
                            g_zombieGazeTimer[i] = (std::max)(0.0f, g_zombieGazeTimer[i] - deltaTime * 2.0f);

                        if (g_zombieGazeTimer[i] >= JUMPSCARE_STARE_DURATION &&
                            currentFrame >= g_jumpscareCooldownUntil && !g_jumpscareActive)
                        {
                            g_jumpscareTriggerTime = currentFrame;
                            g_jumpscareActive = true;
                            g_jumpscareActiveEndTime = currentFrame + JUMPSCARE_EFFECT_DURATION;
                            g_jumpscareCooldownUntil = currentFrame + JUMPSCARE_COOLDOWN;
                            g_zombieGazeTimer[i] = 0.0f;
                            g_startleTriggerTime = currentFrame;
                            playPreloaded("scream");
                            std::cout << "[DEBUG] *** JUMPSCARE *** Zombie " << i << " te asusto de tanto mirarlo." << std::endl;
                        }
                    }

                    glm::vec3 effectivePos(g_zombieCurrentXZ[i].x, inst.worldPos.y, g_zombieCurrentXZ[i].y);
                    float effectiveYaw = ZOMBIE_APPROACH_ENABLED ? g_zombieMoveYaw[i] : inst.yawDeg;

                    float twitchStep = floorf(currentFrame / ZOMBIE_TWITCH_STEP_DURATION + (float)i * 7.0f);
                    float twitchRoll = PseudoRandom01(twitchStep * 4.21f + (float)i * 33.1f);
                    float twitchYaw = (twitchRoll - 0.5f) * 2.0f * ZOMBIE_TWITCH_YAW_DEG;

                    float instPhase = (float)i * 2.399963f;
                    float bobY = sinf(currentFrame * ZOMBIE_TWITCH_BOB_SPEED + instPhase) * ZOMBIE_TWITCH_BOB_AMOUNT;

                    float swayX = sinf(currentFrame * ZOMBIE_SWAY_SPEED + instPhase) * ZOMBIE_SWAY_RADIUS;
                    float swayZ = cosf(currentFrame * ZOMBIE_SWAY_SPEED * 0.8f + instPhase) * ZOMBIE_SWAY_RADIUS * 0.6f;
                    float leanDeg = sinf(currentFrame * ZOMBIE_LEAN_SPEED + instPhase * 1.3f) * ZOMBIE_LEAN_DEG;

                    glm::mat4 instanceMatrix = glm::translate(glm::mat4(1.0f), effectivePos + glm::vec3(swayX, bobY, swayZ));
                    instanceMatrix = glm::rotate(instanceMatrix, glm::radians(effectiveYaw + twitchYaw), glm::vec3(0.0f, 1.0f, 0.0f));
                    instanceMatrix = glm::rotate(instanceMatrix, glm::radians(leanDeg), ZOMBIE_LEAN_AXIS);
                    if (REPEATED_MODEL_IS_Z_UP)
                        instanceMatrix = glm::rotate(instanceMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                    instanceMatrix = glm::scale(instanceMatrix, glm::vec3(inst.scale));
                    instanceMatrix = glm::translate(instanceMatrix, -repeatedPivotLocal);

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, fallbackWhiteTex);

                    if (!zombieParts.empty())
                    {
                        for (auto& part : zombieParts)
                        {
                            glm::mat4 partLocal = glm::mat4(1.0f);

                            if (part.type == 1) // cabeza
                            {
                                float sway = sinf(currentFrame * ZOMBIE_HEAD_SWAY_SPEED + instPhase) * ZOMBIE_HEAD_SWAY_DEG;
                                partLocal = glm::translate(glm::mat4(1.0f), part.pivotLocal);
                                partLocal = glm::rotate(partLocal, glm::radians(sway), ZOMBIE_HEAD_SWAY_AXIS);
                                partLocal = glm::translate(partLocal, -part.pivotLocal);
                            }
                            else if (part.type == 2) // brazo
                            {
                                float armPhase = instPhase + (float)part.meshIndex * 1.7f;
                                float swing = ZOMBIE_ARM_SWING_DEG * (0.5f + 0.5f * sinf(currentFrame * ZOMBIE_ARM_SWING_SPEED + armPhase));
                                partLocal = glm::translate(glm::mat4(1.0f), part.pivotLocal);
                                partLocal = glm::rotate(partLocal, glm::radians(swing), ZOMBIE_ARM_SWING_AXIS);
                                partLocal = glm::translate(partLocal, -part.pivotLocal);
                            }

                            ourShader.setMat4("model", instanceMatrix * partLocal);
                            repeatedModel->meshes[part.meshIndex].Draw(ourShader);
                        }
                    }
                    else
                    {
                        ourShader.setMat4("model", instanceMatrix);
                        repeatedModel->Draw(ourShader);
                    }
                }
            }

            // Bombillas focos techo
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

            // -------- Bombillos de colores del arbol (cada uno con su color) --------
            if (ADD_EXTRA_MODEL && EXTRA_MODEL_ORNAMENT_LIGHTS)
            {
                bulbShader.setFloat("billboardScale", ORNAMENT_LIGHT_BILLBOARD_SCALE);
                glBindVertexArray(bulbVAO);
                for (size_t i = 0; i < g_ornamentLightPositions.size(); i++)
                {
                    bulbShader.setVec3("billboardColor", g_ornamentLightColors[i]);
                    bulbShader.setVec3("billboardCenter", g_ornamentLightPositions[i]);
                    bulbShader.setFloat("billboardIntensity", ornamentIntensity[i] * 1.3f);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
                glBindVertexArray(0);
            }

            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);

            // -------- Imagen de jumpscare a pantalla completa --------
            // Envolvente: sube rapido, se sostiene, y baja -- un "pop" claro,
            // no un fundido lento que perderia el golpe de sorpresa.
            float jumpscareElapsed = currentFrame - g_jumpscareTriggerTime;
            if (jumpscareElapsed >= 0.0f && jumpscareElapsed < JUMPSCARE_IMAGE_DURATION)
            {
                // Golpe brusco, no un fundido: negro instantaneo, unos
                // parpadeos rapidos imagen/negro (glitch), sostenido a full,
                // y recien ahi un corte relativamente rapido. Un jumpscare
                // real no se "desvanece suave", pega y se sostiene.
                const float PRE_BLACK = 0.05f;
                const float STROBE = 0.30f;
                const float HOLD = 0.20f;

                float t = jumpscareElapsed;
                float jumpscareAlpha = 1.0f;
                float blackOverride = 0.0f;

                if (t < PRE_BLACK)
                {
                    blackOverride = 1.0f;
                }
                else if (t < PRE_BLACK + STROBE)
                {
                    float st = t - PRE_BLACK;
                    bool showImage = (fmodf(st * 18.0f, 1.0f) > 0.5f);
                    blackOverride = showImage ? 0.0f : 1.0f;
                }
                else if (t < PRE_BLACK + STROBE + HOLD)
                {
                    blackOverride = 0.0f;
                }
                else
                {
                    float relT = t - (PRE_BLACK + STROBE + HOLD);
                    float relDur = JUMPSCARE_IMAGE_DURATION - (PRE_BLACK + STROBE + HOLD);
                    jumpscareAlpha = 1.0f - glm::clamp(relT / (std::max)(relDur, 0.001f), 0.0f, 1.0f);
                    blackOverride = 0.0f;
                }

                glDisable(GL_DEPTH_TEST);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                jumpscareShader.use();
                jumpscareShader.setInt("jumpscareTex", 0);
                jumpscareShader.setFloat("alpha", jumpscareAlpha);
                jumpscareShader.setFloat("blackOverride", blackOverride);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, jumpscareTex);

                glBindVertexArray(jumpscareVAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);

                glDisable(GL_BLEND);
                glEnable(GL_DEPTH_TEST);
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        haltmusic();
        haltsound();
        closePreloaded("scream");
        closePreloaded("lightfx");
        closePreloaded("ceilingfail");
        closePreloaded("presence");
        closePreloaded("warning");

        // Liberar recursos
        if (extraModel)
        {
            delete extraModel;
            extraModel = nullptr;
        }
        if (repeatedModel)
        {
            delete repeatedModel;
            repeatedModel = nullptr;
        }

        glDeleteVertexArrays(1, &bulbVAO);
        glDeleteBuffers(1, &bulbVBO);
        glDeleteTextures(1, &fallbackWhiteTex);

        glDeleteVertexArrays(1, &jumpscareVAO);
        glDeleteBuffers(1, &jumpscareVBO);
        glDeleteTextures(1, &jumpscareTex);
        glDeleteTextures(1, &mattMessageTex);
    }

    void processInput(GLFWwindow* window)
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // --- BLOQUEO DE CONTROLES SI SE ESTÁ MOSTRANDO LA SALIDA CINEMÁTICA ---
        if (exitPhase != EXIT_NONE) {
            return;
        }

        // Volver al Pasillo
        if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS)
        {
            if (exitPhase == EXIT_NONE) {
                exitPhase = EXIT_WAIT_BLACK;
                exitTimer = static_cast<float>(glfwGetTime());
                haltmusic();
                haltsound();
            }
            return;
        }

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

        if (!g_jumpscareActive && exitPhase == EXIT_NONE) {
            camera.ProcessMouseMovement(xoffset, yoffset);
        }
    }

    void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
    {
        if (exitPhase == EXIT_NONE) {
            camera.ProcessMouseScroll(static_cast<float>(yoffset));
        }
    }
}