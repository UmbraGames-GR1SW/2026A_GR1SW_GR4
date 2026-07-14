#version 330 core
out vec4 FragColor;

uniform vec4 boxColor; // rgb + alpha

void main()
{
    FragColor = boxColor;
}
