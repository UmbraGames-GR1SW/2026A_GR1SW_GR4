#version 330 core
#define MAX_RED_LIGHTS 8

out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D texture_diffuse1;

// Linterna (siempre activa)
uniform vec3 viewPos;
uniform vec3 flashlightDir;
uniform float flashlightCutOff;
uniform float flashlightOuterCutOff;
uniform float flashlightFlicker; // 0..1, simula bateria vieja fallando / apagon real

// Focos de techo: la luz roja sale de puntos reales del modelo, cada uno
// con su propia intensidad (permite parpadeo independiente por lampara)
uniform vec3 redLightPositions[MAX_RED_LIGHTS];
uniform float redLightIntensities[MAX_RED_LIGHTS];
uniform int numRedLights;
uniform vec3 redLightColor;

// Niebla: come la visibilidad lejana
uniform vec3 fogColor;
uniform float fogDensity;

// Vinieta: oscurece los bordes de pantalla
uniform vec2 screenSize;

// Gradacion: la parte "neutra" (ambiente + linterna) se desatura hacia
// gris; el rojo de las lamparas se mantiene intacto y bien saturado.
uniform float desaturation;

void main()
{
    vec3 texColor = texture(texture_diffuse1, TexCoords).rgb;
    vec3 norm = normalize(Normal);

    // --- Parte "neutra": ambiente casi nulo + linterna ---
    vec3 ambient = 0.02 * texColor;

    vec3 flDir = normalize(viewPos - FragPos);
    float flDiff = max(dot(norm, flDir), 0.0);

    float theta = dot(flDir, normalize(-flashlightDir));
    float epsilon = flashlightCutOff - flashlightOuterCutOff;
    float spotIntensity = clamp((theta - flashlightOuterCutOff) / epsilon, 0.0, 1.0);

    float flDistance = length(viewPos - FragPos);
    float flAttenuation = 1.0 / (1.0 + 0.09 * flDistance + 0.032 * flDistance * flDistance);

    vec3 flashlightContribution = flDiff * texColor * flAttenuation * spotIntensity * flashlightFlicker * 1.8;

    vec3 neutral = ambient + flashlightContribution;

    // Desaturar solo la parte neutra (gris frio tipo "found footage")
    float luma = dot(neutral, vec3(0.299, 0.587, 0.114));
    neutral = mix(neutral, vec3(luma), desaturation);

    // --- Parte roja: focos de techo, cada uno con su propia intensidad ---
    vec3 redTotal = vec3(0.0);
    for (int i = 0; i < numRedLights; i++)
    {
        vec3 toLight = redLightPositions[i] - FragPos;
        float dist = length(toLight);
        vec3 lightDir = toLight / max(dist, 0.001);

        float diff = max(dot(norm, lightDir), 0.0);
        // Atenuacion fuerte: cada foco ilumina una zona limitada debajo
        // suyo, como una lampara real de techo, no todo el cuarto parejo.
        float attenuation = 1.0 / (1.0 + 0.28 * dist + 0.13 * dist * dist);

        redTotal += diff * attenuation * texColor * redLightIntensities[i];
    }
    vec3 red = redLightColor * redTotal;

    vec3 result = neutral + red;

    // --- Niebla: mientras mas lejos, mas se funde a negro/rojizo ---
    float fogFactor = 1.0 - exp(-fogDensity * flDistance * flDistance);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    result = mix(result, fogColor, fogFactor);

    // --- Vinieta ---
    vec2 uv = gl_FragCoord.xy / screenSize;
    float vig = smoothstep(0.9, 0.35, length(uv - vec2(0.5)));
    result *= mix(0.3, 1.0, vig);

    FragColor = vec4(result, 1.0);
}