#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
 
uniform sampler2D jumpscareTex;
uniform float alpha;
uniform float blackOverride; // 0 = imagen normal, 1 = negro puro (para el flash previo/estrobo)
 
void main()
{
    vec3 texColor = texture(jumpscareTex, TexCoord).rgb;
    vec3 finalColor = mix(texColor, vec3(0.0), blackOverride);
    FragColor = vec4(finalColor, alpha);
}
 