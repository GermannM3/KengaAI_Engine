// Упрощенный фрагментный шейдер для тестирования совместимости
#version 400 core

// Input from vertex shader
in vec3 vWorldPos;
in vec3 vColor;
in vec3 vNormal;
in vec2 vTexCoord;

// Output color
out vec4 FragColor;

// Uniform data
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uViewPos;

void main() {
    // Simple diffuse lighting
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(uLightPos - vWorldPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * uLightColor;
    
    // Simple ambient
    vec3 ambient = 0.1 * uLightColor;
    
    // Combine lighting
    vec3 result = (ambient + diffuse) * vColor;
    
    // Simple texture sampling (if needed)
    FragColor = vec4(result, 1.0);
}