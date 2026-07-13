#version 330 core
in vec2 vLocalPos;
out vec4 FragColor;

uniform vec3 billboardColor;
uniform float billboardIntensity;

void main()
{
    // Glow radial: brillante en el centro, se apaga suave hacia el borde
    // (asi se ve como una bombilla/lampara, no un cuadrado rojo).
    float d = length(vLocalPos) * 2.0; // 0 en el centro, 1 en el borde
    float glow = smoothstep(1.0, 0.0, d);
    glow = pow(glow, 1.6);
    vec3 color = billboardColor * billboardIntensity * glow;
    FragColor = vec4(color, glow);
}