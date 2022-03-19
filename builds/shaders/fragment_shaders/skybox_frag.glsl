#version 440

layout(location = 0) in vec3 inNormal;

uniform layout (binding=15) samplerCube s_Environment;

layout(location = 0) out vec4 color;

void main() {
    vec3 norm = normalize(inNormal);

    color = vec4(texture(s_Environment, norm).rgb, 0.0);
}