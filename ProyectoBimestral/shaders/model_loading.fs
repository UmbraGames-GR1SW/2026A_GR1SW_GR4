#version 330 core
#define MAX_RED_LIGHTS 9

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

// Material: que tan "brilloso/mojado" se ve todo bajo las luces
uniform float materialShininess;

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
    vec3 viewDir = normalize(viewPos - FragPos);

    // --- Parte "neutra": ambiente casi nulo + linterna (difusa + especular) ---
    vec3 ambient = 0.02 * texColor;

    // Para la linterna, la luz sale de la MISMA posicion que la camara,
    // asi que lightDir y viewDir son el mismo vector. El especular igual
    // funciona: crea un lobulo mucho mas angosto que la difusa, dando un
    // brillo/destello notorio en superficies lisas (metal, charcos, vidrio).
    vec3 flLightDir = viewDir;
    float flDiff = max(dot(norm, flLightDir), 0.0);

    vec3 flHalfway = normalize(flLightDir + viewDir); // == flLightDir en este caso, pero se deja explicito
    float flSpec = pow(max(dot(norm, flHalfway), 0.0), materialShininess);

    float theta = dot(flLightDir, normalize(-flashlightDir));
    float epsilon = flashlightCutOff - flashlightOuterCutOff;
    float spotIntensity = clamp((theta - flashlightOuterCutOff) / epsilon, 0.0, 1.0);

    float flDistance = length(viewPos - FragPos);
    float flAttenuation = 1.0 / (1.0 + 0.09 * flDistance + 0.032 * flDistance * flDistance);

    vec3 flDiffuseTerm = flDiff * texColor;
    vec3 flSpecularTerm = vec3(flSpec) * 0.6; // blanco, no depende de la textura (brillo especular puro)

    vec3 flashlightContribution = (flDiffuseTerm * 1.8 + flSpecularTerm) * flAttenuation * spotIntensity * flashlightFlicker;

    vec3 neutral = ambient + flashlightContribution;

    // Desaturar solo la parte neutra (gris frio tipo "found footage")
    float luma = dot(neutral, vec3(0.299, 0.587, 0.114));
    neutral = mix(neutral, vec3(luma), desaturation);

    // --- Parte roja: focos de techo, difusa + especular, cada uno con su propia intensidad ---
    vec3 redDiffuseTotal = vec3(0.0);
    vec3 redSpecularTotal = vec3(0.0);
    for (int i = 0; i < numRedLights; i++)
    {
        vec3 toLight = redLightPositions[i] - FragPos;
        float dist = length(toLight);
        vec3 lightDir = toLight / max(dist, 0.001);

        // Piso minimo de 0.12: aunque la superficie no mire directo a la
        // lampara, sigue recibiendo un toque de rojo -- asi el efecto se
        // "lee" claramente en vez de desaparecer en superficies de perfil.
        float diff = max(dot(norm, lightDir), 0.12);

        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), materialShininess);

        // Atenuacion mas suave que antes: el charco de luz se nota desde
        // mas lejos, sin llegar a banar todo el cuarto parejo.
        float attenuation = 1.0 / (1.0 + 0.14 * dist + 0.05 * dist * dist);

        redDiffuseTotal += diff * attenuation * texColor * redLightIntensities[i];
        redSpecularTotal += spec * attenuation * redLightIntensities[i] * 0.5;
    }
    vec3 red = redLightColor * redDiffuseTotal + redLightColor * redSpecularTotal;

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