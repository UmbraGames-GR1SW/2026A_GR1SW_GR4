#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D texture_diffuse1;

uniform vec3 viewPos;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform float specularStrength;

// =====================================
// LINTERNA
// =====================================
struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform SpotLight spotLight;

// =====================================
// LUCES ROJAS DE LOS LETREROS (20 en total)
// =====================================
#define NR_POINT_LIGHTS 20
struct PointLight {
    vec3 position;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform PointLight pointLights[NR_POINT_LIGHTS];

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 texColor) {
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    vec3 ambient = light.ambient * texColor;
    vec3 diffuse = light.diffuse * diff * texColor;
    vec3 specular = light.specular * spec;
    
    return (ambient + diffuse + specular) * attenuation * intensity;
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 texColor) {
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    vec3 ambient = light.ambient * texColor;
    vec3 diffuse = light.diffuse * diff * texColor;
    vec3 specular = light.specular * spec;
    
    return (ambient + diffuse + specular) * attenuation;
}

void main() {
    vec4 texColorRGBA = texture(texture_diffuse1, TexCoords);
    if (texColorRGBA.a < 0.5) discard;
    vec3 texColor = texColorRGBA.rgb;

    // --- Ambiente Base ---
    float ambientStrength = 0.01; 
    vec3 ambient = ambientStrength * texColor;

    // --- Luz Global ---
    vec3 norm = normalize(Normal);
    vec3 lightDirection = normalize(-lightDir);
    float diff = max(dot(norm, lightDirection), 0.0);
    vec3 diffuse = diff * texColor * lightColor; 

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightDirection + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 64.0);
    vec3 specular = spec * specularStrength * vec3(1.0, 1.0, 1.0);

    // --- Mezcla Principal ---
    vec3 result = ambient + diffuse + specular;

    // Sumar linterna
    result += CalcSpotLight(spotLight, norm, FragPos, viewDir, texColor);

    // Sumar todos los puntos de luz rojos
    for(int i = 0; i < NR_POINT_LIGHTS; i++) {
        result += CalcPointLight(pointLights[i], norm, FragPos, viewDir, texColor);
    }
    
    // =====================================
    // LIMITAR ALCANCE DE VISIÓN (NIEBLA NEGRA)
    // =====================================
    float fogNear = 7.0;  // Distancia a la que empieza a oscurecerse la visión
    float fogFar = 25.0;  // Distancia donde ya no ves absolutamente nada (negro total)
    vec3 fogColor = vec3(0.0, 0.0, 0.0); // Niebla de color negro
    
    // Calculamos la distancia real entre tu cámara y el píxel que se está dibujando
    float dist = length(viewPos - FragPos);
    
    // Calculamos qué tan intensa debe ser la oscuridad
    float fogFactor = (fogFar - dist) / (fogFar - fogNear);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    
    // Mezclamos tu escena con el negro absoluto según la distancia
    result = mix(fogColor, result, fogFactor);

    FragColor = vec4(result, texColorRGBA.a);
}