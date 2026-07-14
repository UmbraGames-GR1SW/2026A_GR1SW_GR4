#version 330 core
#define MAX_RED_LIGHTS 9
#define MAX_ORNAMENT_LIGHTS 10

out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D texture_diffuse1;

uniform vec3 viewPos;
uniform vec3 flashlightDir;
uniform float flashlightCutOff;
uniform float flashlightOuterCutOff;
uniform float flashlightFlicker;

uniform vec3 redLightPositions[MAX_RED_LIGHTS];
uniform float redLightIntensities[MAX_RED_LIGHTS];
uniform int numRedLights;
uniform vec3 redLightColor;

// Bombillos de colores del arbol: cada uno con su propio color e intensidad
uniform vec3 ornamentLightPositions[MAX_ORNAMENT_LIGHTS];
uniform vec3 ornamentLightColors[MAX_ORNAMENT_LIGHTS];
uniform float ornamentLightIntensities[MAX_ORNAMENT_LIGHTS];
uniform int numOrnamentLights;

uniform float materialShininess;

uniform vec3 fogColor;
uniform float fogDensity;

uniform vec2 screenSize;

uniform float desaturation;

uniform float uTime;
uniform float contrastPower;
uniform float grainAmount;

// Golpe visual breve: 0 = nada, 1 = flash total. Se dispara desde C++
// en el instante justo en que la linterna se corta.
uniform float startleFlash;

void main()
{
    vec3 texColor = texture(texture_diffuse1, TexCoords).rgb;
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    vec3 flashlightTint = vec3(0.75, 0.86, 1.0);

    vec3 ambient = 0.006 * texColor;

    vec3 flLightDir = viewDir;
    float flDiff = max(dot(norm, flLightDir), 0.0);
    float flSpec = pow(max(dot(norm, flLightDir), 0.0), materialShininess);

    float theta = dot(flLightDir, normalize(-flashlightDir));
    float epsilon = flashlightCutOff - flashlightOuterCutOff;
    float spotIntensity = clamp((theta - flashlightOuterCutOff) / epsilon, 0.0, 1.0);

    float flDistance = length(viewPos - FragPos);
    float flAttenuation = 1.0 / (1.0 + 0.07 * flDistance + 0.020 * flDistance * flDistance);

    vec3 flDiffuseTerm = flDiff * texColor * flashlightTint;
    vec3 flSpecularTerm = vec3(flSpec) * 0.6 * flashlightTint;

    vec3 flashlightContribution = (flDiffuseTerm * 2.6 + flSpecularTerm) * flAttenuation * spotIntensity * flashlightFlicker;

    vec3 neutral = ambient + flashlightContribution;

    float luma = dot(neutral, vec3(0.299, 0.587, 0.114));
    vec3 sicklyTint = vec3(luma) * vec3(0.82, 1.0, 0.85);
    neutral = mix(neutral, sicklyTint, desaturation);

    neutral = pow(max(neutral, vec3(0.0)), vec3(contrastPower));

    vec3 redDiffuseTotal = vec3(0.0);
    vec3 redSpecularTotal = vec3(0.0);
    for (int i = 0; i < numRedLights; i++)
    {
        vec3 toLight = redLightPositions[i] - FragPos;
        float dist = length(toLight);
        vec3 lightDir = toLight / max(dist, 0.001);

        float diff = max(dot(norm, lightDir), 0.15);
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), materialShininess);

        float attenuation = 1.0 / (1.0 + 0.11 * dist + 0.035 * dist * dist);

        redDiffuseTotal += diff * attenuation * texColor * redLightIntensities[i];
        redSpecularTotal += spec * attenuation * redLightIntensities[i] * 0.6;
    }
    vec3 red = redLightColor * redDiffuseTotal + redLightColor * redSpecularTotal;

    float flashlightLuma = dot(flashlightContribution, vec3(0.299, 0.587, 0.114));
    float redSuppression = 1.0 - clamp(flashlightLuma * 1.4, 0.0, 0.85);
    red *= redSuppression;

    // Bombillos de colores del arbol: pequenos, rango corto, cada uno con
    // su propio color -- se suman sin supresion (son acentos decorativos,
    // no compiten con la linterna de la misma manera que el rojo del techo).
    vec3 ornaments = vec3(0.0);
    for (int i = 0; i < numOrnamentLights; i++)
    {
        vec3 toLight = ornamentLightPositions[i] - FragPos;
        float dist = length(toLight);
        vec3 lightDir = toLight / max(dist, 0.001);

        float diff = max(dot(norm, lightDir), 0.2);
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), materialShininess);

        // Rango corto: son bombillos chicos, no deben iluminar todo el cuarto
        float attenuation = 1.0 / (1.0 + 0.6 * dist + 0.9 * dist * dist);

        vec3 contribution = (diff * texColor + vec3(spec) * 0.5) * attenuation * ornamentLightIntensities[i];
        ornaments += ornamentLightColors[i] * contribution;
    }

    vec3 result = neutral + red + ornaments;

    float fogFactor = 1.0 - exp(-fogDensity * flDistance * flDistance);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    result = mix(result, fogColor, fogFactor);

    // Grano un poco mas marcado
    float grainSeed = dot(gl_FragCoord.xy, vec2(12.9898, 78.233)) + uTime * 43.0;
    float grain = fract(sin(grainSeed) * 43758.5453) - 0.5;
    result += grain * grainAmount;

    // Vinieta con tinte rojizo sucio en los bordes en vez de negro puro
    vec2 uv = gl_FragCoord.xy / screenSize;
    float vig = smoothstep(0.75, 0.20, length(uv - vec2(0.5)));
    vec3 vigTint = vec3(0.05, 0.0, 0.0);
    result = mix(result * 0.08 + vigTint, result, vig);

    // Golpe visual: flash rojo breve, se desvanece rapido
    result = mix(result, vec3(0.55, 0.02, 0.02), startleFlash);

    FragColor = vec4(max(result, vec3(0.0)), 1.0);
}