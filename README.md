# El Pabellón 9 — Simulador de Recuerdos

Proyecto de Computación Gráfica — **2026A_GR1SW_GR4**

Un videojuego de terror psicológico en primera persona desarrollado con OpenGL, donde cada sala es un recuerdo fragmentado que el jugador debe atravesar para descubrir la verdad.

---

## Umbra Games

**Integrantes:**
- Josué Delgado
- Samantha Lozada
- Anahí Pillajo
- Matthew Redin

### Quiénes somos

En Umbra Games diseñamos experiencias de terror interactivo que van más allá del susto fácil. Creemos que el miedo, cuando se construye con intención narrativa, se convierte en una de las herramientas más poderosas para contar historias humanas: sobre el duelo, la culpa, el trauma y la memoria.

### Misión

Desarrollar videojuegos que generen tensión psicológica real, con narrativas profundas y personajes con los que el jugador pueda conectar emocionalmente, combinando diseño de sonido, ambientación y mecánicas de juego de calidad para ofrecer experiencias inmersivas que permanezcan con el jugador mucho después de haber apagado la consola. Entendemos que el buen terror no se trata de sorprender, sino de hacer sentir.

### Visión

Para el 2030, Umbra Games aspira consolidarse como el estudio independiente de terror psicológico más influyente de Latinoamérica, reconocido internacionalmente por su capacidad de fusionar narrativa emocional, diseño artístico distintivo e innovación tecnológica en cada uno de sus títulos. Buscamos expandir nuestro catálogo a nuevas plataformas y mercados, construir una comunidad de jugadores fieles que valoren el terror como una forma legítima de arte interactivo, y convertirnos en un referente para nuevos desarrolladores que quieran contar historias oscuras con propósito.

---

## El juego

### Premisa

Elena despierta en el Pabellón 9 sin recordar por qué está en ese lugar. Cada puerta esconde un fragmento de su mente que ella misma enterró para sobrevivir. Para trascender, primero debe recordar.

### Las 4 salas — fragmentos de una mente rota

| Sala | Recuerdo | Escena (código) |
|---|---|---|
| **1 — El Garage** | El recuerdo más reciente y más doloroso: una discusión con su esposo que se salió de control, y una verdad que Elena no quiere ver. Su mente lo bloqueó para protegerse de saber que ella fue la causante. | `Scene_Samy.cpp` |
| **2 — El Cuarto de la Infancia** | Payasos, muñecos poseídos, máscaras. Elena aprendió de niña a convertir el miedo real en algo "de mentira" — un mecanismo de defensa que usó toda su vida para no sentir. | `Scene_Anahi.cpp` |
| **3 — El Almacén Navideño** | El último recuerdo feliz con su esposo antes del incidente, distorsionado por su propia mente en algo grotesco, porque recordar que ya no será es demasiado doloroso. | `main_warehouse.cpp` |
| **4 — El Pasillo** | Una entidad la persigue sin cansarse jamás, pasillo tras pasillo. No es un monstruo: es la culpa. Mientras Elena siga corriendo, nunca dejará de perseguirla. | `Scene_Josue.cpp` |

Todas las salas se conectan a través de un **pasillo central** (`Scene_Pasillo.cpp`) que funciona como hub: desde ahí se accede progresivamente a cada sala a medida que se desbloquean.

### La verdad

> Elena se suicidó porque no soportó que ella fue la causante del asesinato de su esposo. El "hospital psiquiátrico" nunca existió: es el purgatorio donde ella libra una lucha interna a través de sus recuerdos por lo que hizo. La entidad que la persigue no es nada más que su propia conciencia, que no la dejará trascender. Elena deberá aceptar su realidad para poder salir de ese lugar.

El jugador se entera de la verdad completa tras recorrer las 4 salas.

---

## Arquitectura del proyecto

### Stack técnico

| Componente | Uso |
|---|---|
| **OpenGL 3.3 Core** | Renderizado 3D |
| **GLFW** | Ventana, input (teclado/mouse) |
| **GLAD** | Carga de funciones OpenGL |
| **Assimp** | Carga de modelos 3D (`.obj`) |
| **FreeType** | Renderizado de texto (HUD) |
| **stb_image** | Carga de texturas |
| **GLM** | Matemática (vectores, matrices, transformaciones) |
| **WinMM (MCI)** | Reproducción de audio (música y efectos de sonido) |

### Sistema de escenas

El proyecto usa un **enrutador de escenas** central (`Source.cpp`) que arranca la ventana una sola vez y va delegando el control a cada escena según corresponda:

```cpp
enum SceneType { SCENE_PASILLO, SCENE_SAMY, SCENE_ANI, SCENE_MATTHEW, SCENE_JOSUE };
```

Cada escena vive en su propio namespace con una función `RunScene(GLFWwindow* window)` que contiene su propio bucle de render, su propia configuración de shaders/modelos/audio, y su propia lógica de input. Al terminar (por ejemplo, al presionar `H` para volver), cambia `g_NextScene` y devuelve el control al enrutador, que carga la siguiente escena.

Esto permite que cada integrante trabaje su sala de forma independiente sin pisar el código de los demás, mientras comparten la misma base de librerías y assets.

### Estructura de carpetas

```
2026A_GR1SW_GR4/
├── ProyectoBimestral/              # Código fuente y assets del juego
│   ├── Source.cpp                  # Punto de entrada, enrutador de escenas
│   ├── Scene_Pasillo.cpp           # Hub central
│   ├── Scene_Samy.cpp              # Sala 1 — El Garage
│   ├── Scene_Anahi.cpp             # Sala 2 — El Cuarto de la Infancia
│   ├── main_warehouse.cpp          # Sala 3 — El Almacén Navideño
│   ├── Scene_Josue.cpp             # Sala 4 — El Pasillo
│   ├── glad.c
│   ├── text_renderer.h
│   ├── shaders/                    # Shaders GLSL (.vs / .fs) de todas las escenas
│   ├── model/                      # Modelos 3D (.obj/.mtl) por escena/objeto
│   ├── fonts/                      # Tipografías (.ttf) para el HUD
│   ├── music/ y audios/            # Música ambiental y efectos de sonido
│   └── ProyectoBimestral.vcxproj
│
├── ProyectoBimestral_Stuff/        # Dependencias compartidas por todo el equipo
│   ├── include/
│   │   ├── learnopengl/            # shader.h, camera.h, model.h, mesh.h, audio.h, text_renderer.h, stb_image.h
│   │   ├── glad/, glm/, freetype/  # Librerías de terceros (headers)
│   └── Library/                    # .lib y .dll precompilados (assimp, freetype, glfw3)
│
├── ProyectoBimestral.slnx          # Solución de Visual Studio
└── README.md
```

### Mecánicas comunes entre escenas

- **Cámara en primera persona** (mouse look + WASD), basada en `learnopengl/camera.h`.
- **Colisión por grilla 2D**: cada escena construye una grilla de colisión a partir de los triángulos del modelo cargado (separando piso/techo por franja de altura), sin necesidad de hitboxes manuales.
- **HUD de texto**: controles e información de sala en pantalla (`learnopengl/text_renderer.h`, FreeType), ocultable con `M`.
- **Sistema de audio con `winmm`/MCI**: música de fondo en loop (`playmusic`) y efectos de sonido precargados con alias propios (`preloadSound` / `playPreloaded`) para poder sonar simultáneamente sin cortarse entre sí.
- **Navegación entre escenas**: tecla `H` para volver al pasillo principal desde cualquier sala.

---

## Cómo compilar y correr

1. Abrir `ProyectoBimestral.slnx` con Visual Studio 2022.
2. Configuración `Debug` / plataforma `x64`.
3. Las dependencias precompiladas (Assimp, FreeType, GLFW) ya están en `ProyectoBimestral_Stuff/Library/`. **Importante**: hace falta que `assimp-vc145-mtd.dll` esté físicamente en la misma carpeta que el `.exe` compilado (`x64/Debug/`) para poder ejecutar — si no está, agregar un evento posterior a compilación que la copie automáticamente, o copiarla a mano la primera vez.
4. Compilar y ejecutar con `F5`.

---

## Controles generales

| Tecla | Acción |
|---|---|
| `W A S D` | Moverse |
| Mouse | Mirar alrededor |
| `M` | Mostrar/ocultar HUD de controles |
| `H` | Volver al pasillo principal |
| `Esc` | Salir |

(Cada sala puede tener controles adicionales propios — ver el HUD en pantalla dentro de cada una.)
