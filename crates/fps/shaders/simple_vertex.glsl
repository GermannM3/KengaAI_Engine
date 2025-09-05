// Упрощенный вертексный шейдер для тестирования совместимости
#version 400 core

// Uniform data
uniform mat4 uViewProj;

// Vertex input
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aInstancePos;
layout(location = 4) in vec3 aInstanceScale;
layout(location = 5) in float aInstanceRotY;
layout(location = 6) in vec3 aInstanceColor;

// Output to fragment shader
out vec3 vWorldPos;
out vec3 vColor;
out vec3 vNormal;
out vec2 vTexCoord;

// Rotation matrix function
mat4 rotationY(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat4(
        vec4(c, 0.0, s, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(-s, 0.0, c, 0.0),
        vec4(0.0, 0.0, 0.0, 1.0)
    );
}

void main() {
    // Create model matrix
    mat4 scaleMat = mat4(
        vec4(aInstanceScale.x, 0.0, 0.0, 0.0),
        vec4(0.0, aInstanceScale.y, 0.0, 0.0),
        vec4(0.0, 0.0, aInstanceScale.z, 0.0),
        vec4(0.0, 0.0, 0.0, 1.0)
    );
    
    mat4 rotMat = rotationY(aInstanceRotY);
    
    mat4 transMat = mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(aInstancePos, 1.0)
    );
    
    mat4 modelMat = transMat * rotMat * scaleMat;
    
    // Transform vertex
    vec4 worldPos = modelMat * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vColor = aInstanceColor;
    vNormal = (rotMat * vec4(aNormal, 0.0)).xyz;
    vTexCoord = aTexCoord;
    
    gl_Position = uViewProj * worldPos;
}