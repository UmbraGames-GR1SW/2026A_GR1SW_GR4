#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D texture_diffuse1;

// Camara / linterna
uniform vec3 viewPos;
uniform vec3 flashlightDir;
uniform bool flashlightOn;
uniform float flashlightCutOff;      // coseno del angulo interior del cono
uniform float flashlightOuterCutOff; // coseno del angulo exterior del cono

// Luz roja ambiental que palpita (modo terror sin linterna)
uniform vec3 redLightColor;
uniform float redLightIntensity;

void main()
{
    vec3 texColor = texture(texture_diffuse1, TexCoords).rgb;
    vec3 norm = normalize(Normal);

    // Ambiente muy bajo para que nunca quede 100% negro (mantiene la sensacion
    // de oscuridad pero sin perder la silueta de los objetos)
    vec3 ambient = 0.03 * texColor;
    vec3 result = ambient;

    if (flashlightOn)
    {
        vec3 lightDir = normalize(viewPos - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);

        float theta = dot(lightDir, normalize(-flashlightDir));
        float epsilon = flashlightCutOff - flashlightOuterCutOff;
        float spotIntensity = clamp((theta - flashlightOuterCutOff) / epsilon, 0.0, 1.0);

        float distance = length(viewPos - FragPos);
        float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);

        vec3 diffuse = diff * texColor * attenuation * spotIntensity;
        result += diffuse * 1.8; // linterna con punch para que se note en la oscuridad
    }
    else
    {
        // Luz roja sin direccion fija (glow ambiental), pulsando en intensidad
        float ndotUp = max(dot(norm, vec3(0.0, 1.0, 0.0)), 0.15);
        vec3 redGlow = redLightColor * redLightIntensity * texColor * ndotUp;
        result += redGlow;
    }

    FragColor = vec4(result, 1.0);
}