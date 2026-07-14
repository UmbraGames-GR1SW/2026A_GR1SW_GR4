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
uniform float flashlightFlicker;

// Focos de techo
uniform vec3 redLightPositions[MAX_RED_LIGHTS];
uniform float redLightIntensities[MAX_RED_LIGHTS];
uniform int numRedLights;
uniform vec3 redLightColor;

uniform float materialShininess;

// Niebla
uniform vec3 fogColor;
uniform float fogDensity;

// Vinieta
uniform vec2 screenSize;

// Gradacion
uniform float desaturation;

// Post-proceso de terror
uniform float uTime;
uniform float contrastPower;
uniform float grainAmount;

void main()
{
    vec3 texColor = texture(texture_diffuse1, TexCoords).rgb;
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // Tinte frio para la linterna (blanco-azulado), bien distinto del
    // rojo calido de las lamparas -- esto es lo que evita que se
    // confundan cuando se solapan en la misma superficie.
    vec3 flashlightTint = vec3(0.75, 0.86, 1.0);

    vec3 ambient = 0.008 * texColor;

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

    // Focos rojos de techo
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

    // Donde la linterna ya pega fuerte, atenuamos el rojo en vez de sumarlo
    // sin mas -- evita el mezclado rosado/naranja y hace que se sientan
    // como dos fuentes de luz claramente distintas, no una mezcla ambigua.
    float flashlightLuma = dot(flashlightContribution, vec3(0.299, 0.587, 0.114));
    float redSuppression = 1.0 - clamp(flashlightLuma * 1.4, 0.0, 0.85);
    red *= redSuppression;

    vec3 result = neutral + red;

    float fogFactor = 1.0 - exp(-fogDensity * flDistance * flDistance);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    result = mix(result, fogColor, fogFactor);

    float grainSeed = dot(gl_FragCoord.xy, vec2(12.9898, 78.233)) + uTime * 43.0;
    float grain = fract(sin(grainSeed) * 43758.5453) - 0.5;
    result += grain * grainAmount;

    vec2 uv = gl_FragCoord.xy / screenSize;
    float vig = smoothstep(0.75, 0.22, length(uv - vec2(0.5)));
    result *= mix(0.10, 1.0, vig);

    FragColor = vec4(max(result, vec3(0.0)), 1.0);
}