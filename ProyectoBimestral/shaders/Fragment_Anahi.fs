#version 330 core
out vec4 FragColor;

struct PointLight {
    vec3 position;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

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

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 viewPos;
uniform PointLight pointLight;
uniform SpotLight spotLight;

uniform sampler2D texture_diffuse1;
const float shininess = 32.0;

void main()
{
    // Muestrear la textura del archivo
    vec4 sampledColor = texture(texture_diffuse1, TexCoords);
    
    // Si la textura falla o no se encuentra (rgb igual a 0), asignamos un gris base
    vec3 texColor = (sampledColor.rgb == vec3(0.0)) ? vec3(0.5, 0.5, 0.5) : sampledColor.rgb;

    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // -------------------------------------------------------------------------
    // CÁLCULO POINT LIGHT (Foco Fijo en el Techo)
    // -------------------------------------------------------------------------
    vec3 lightDir = normalize(pointLight.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    
    float distance = length(pointLight.position - FragPos);
    float attenuation = 1.0 / (pointLight.constant + pointLight.linear * distance + pointLight.quadratic * (distance * distance));

    vec3 ambientP = pointLight.ambient * texColor * attenuation;
    vec3 diffuseP = pointLight.diffuse * diff * texColor * attenuation;
    vec3 specularP = pointLight.specular * spec * attenuation;

    // -------------------------------------------------------------------------
    // CÁLCULO SPOTLIGHT (Tu Linterna Interactiva)
    // -------------------------------------------------------------------------
    vec3 spotLightDir = normalize(spotLight.position - FragPos);
    float spotDiff = max(dot(norm, spotLightDir), 0.0);
    vec3 spotReflectDir = reflect(-spotLightDir, norm);
    float spotSpec = pow(max(dot(viewDir, spotReflectDir), 0.0), shininess);

    float spotDistance = length(spotLight.position - FragPos);
    float spotAttenuation = 1.0 / (spotLight.constant + spotLight.linear * spotDistance + spotLight.quadratic * (spotDistance * spotDistance));

    float theta = dot(spotLightDir, normalize(-spotLight.direction));
    float epsilon = spotLight.cutOff - spotLight.outerCutOff;
    float intensity = clamp((theta - spotLight.outerCutOff) / epsilon, 0.0, 1.0);

    vec3 ambientS = spotLight.ambient * texColor * spotAttenuation * intensity;
    vec3 diffuseS = spotLight.diffuse * spotDiff * texColor * spotAttenuation * intensity;
    vec3 specularS = spotLight.specular * spotSpec * spotAttenuation * intensity;

    // Suma combinada de la bombilla y la linterna
    vec3 result = (ambientP + diffuseP + specularP) + (ambientS + diffuseS + specularS);
    FragColor = vec4(result, 1.0);
}