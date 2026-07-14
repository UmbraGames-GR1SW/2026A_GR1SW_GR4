#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D jumpscareTex;
uniform float alpha;

void main()
{
    vec3 texColor = texture(jumpscareTex, TexCoord).rgb;
    FragColor = vec4(texColor, alpha);
}
