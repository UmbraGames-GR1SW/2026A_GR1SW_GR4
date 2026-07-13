#version 330 core
layout (location = 0) in vec2 aPos; // quad local, -0.5..0.5

uniform vec3 billboardCenter;
uniform vec3 camRight;
uniform vec3 camUp;
uniform float billboardScale;
uniform mat4 view;
uniform mat4 projection;

out vec2 vLocalPos;

void main()
{
    vLocalPos = aPos;
    vec3 worldPos = billboardCenter
                   + camRight * aPos.x * billboardScale
                   + camUp    * aPos.y * billboardScale;
    gl_Position = projection * view * vec4(worldPos, 1.0);
}