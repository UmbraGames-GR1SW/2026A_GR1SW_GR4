#version 330 core
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
uniform float flashlightFlicker; // 0..1, simula bateria vieja fallando

// Glow rojo de fondo (siempre presente, tenue, palpita)
uniform vec3 redLightColor;
uniform float redLightIntensity;

// Niebla: come la visibilidad lejana
uniform vec3 fogColor;
uniform float fogDensity;

// Vinieta: oscurece los bordes de pantalla
uniform vec2 screenSize;

void main()
{
    vec3 texColor = texture(texture_diffuse1, TexCoords).rgb;
    vec3 norm = normalize(Normal);

    // Ambiente casi nulo: la escena depende casi por completo de la linterna
    vec3 ambient = 0.02 * texColor;
    vec3 result = ambient;

    // --- Linterna con parpadeo ---
    vec3 lightDir = normalize(viewPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);

    float theta = dot(lightDir, normalize(-flashlightDir));
    float epsilon = flashlightCutOff - flashlightOuterCutOff;
    float spotIntensity = clamp((theta - flashlightOuterCutOff) / epsilon, 0.0, 1.0);

    float distance = length(viewPos - FragPos);
    float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);

    vec3 flashlightContribution = diff * texColor * attenuation * spotIntensity * flashlightFlicker * 1.8;
    result += flashlightContribution;

    // --- Glow rojo de fondo (siempre presente, no depende de la linterna) ---
    float ndotUp = max(dot(norm, vec3(0.0, 1.0, 0.0)), 0.1);
    vec3 redGlow = redLightColor * redLightIntensity * texColor * ndotUp;
    result += redGlow;

    // --- Niebla: mientras mas lejos, mas se funde a negro/rojizo ---
    float fogFactor = 1.0 - exp(-fogDensity * distance * distance);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    result = mix(result, fogColor, fogFactor);

    // --- Vinieta ---
    vec2 uv = gl_FragCoord.xy / screenSize;
    float vig = smoothstep(0.9, 0.35, length(uv - vec2(0.5)));
    result *= mix(0.3, 1.0, vig);

    FragColor = vec4(result, 1.0);
}