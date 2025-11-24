#version 330 core

out vec4 frag_color;

uniform sampler2D shadow_map;
uniform mat4 world_to_light_ndc_matrix;
uniform bool use_shadow_mapping;

struct AmbientLight {
    bool enabled;
    vec3 ambient;
};

struct PointLight {
    bool enabled;
    vec3 position;
    vec3 diffuse;
    vec3 specular;
    vec3 attenuation;
};

struct DirectionalLight {
    bool enabled;
    vec3 direction;
    vec3 diffuse;
    vec3 specular;
};
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

in vec3 world_position;
in vec3 world_normal;
in vec2 tex_coord;

uniform vec3 camera_position;

uniform Material material;
uniform AmbientLight ambient_light;
uniform PointLight point_light; 
uniform DirectionalLight directional_light;

uniform sampler2D ambient_texture;
uniform sampler2D diffuse_texture;
uniform sampler2D specular_texture;

uniform bool ambient_use_texture;
uniform bool diffuse_use_texture;
uniform bool specular_use_texture;

// Forward declarations
vec3 CalcAmbientLight();
vec3 CalcPointLight(vec3 normal, vec3 view_dir);
vec3 CalcDirectionalLight(vec3 normal, vec3 view_dir);
float CalculateShadow(vec3 world_pos);  // Add this forward declaration

void main() {
    vec3 normal = normalize(world_normal);
    vec3 view_dir = normalize(camera_position - world_position);

    frag_color = vec4(0.0);

    if (ambient_light.enabled) {
        frag_color += vec4(CalcAmbientLight(), 1.0);
    }
    
    if (point_light.enabled) {
        frag_color += vec4(CalcPointLight(normal, view_dir), 1.0);
    }

    if (directional_light.enabled) {
        frag_color += vec4(CalcDirectionalLight(normal, view_dir), 1.0);
    }
}

vec3 GetAmbientColor() {
    if (ambient_use_texture) {
        return texture(ambient_texture, tex_coord).rgb;
    }
    return material.ambient;
}

vec3 GetDiffuseColor() {
    if (diffuse_use_texture) {
        return texture(diffuse_texture, tex_coord).rgb;
    }
    return material.diffuse;
}

vec3 GetSpecularColor() {
    if (specular_use_texture) {
        return texture(specular_texture, tex_coord).rgb;
    }
    return material.specular;
}

vec3 CalcAmbientLight() {
    return ambient_light.ambient * GetAmbientColor();
}

vec3 CalcPointLight(vec3 normal, vec3 view_dir) {
    PointLight light = point_light;
    vec3 light_dir = normalize(light.position - world_position);

    float diffuse_intensity = max(dot(normal, light_dir), 0.0);
    vec3 diffuse_color = diffuse_intensity * light.diffuse * GetDiffuseColor();

    vec3 reflect_dir = reflect(-light_dir, normal);
    float specular_intensity = pow(
        max(dot(view_dir, reflect_dir), 0.0), material.shininess);
    vec3 specular_color = specular_intensity * 
        light.specular * GetSpecularColor();

    float distance = length(light.position - world_position);
    float attenuation = 1.0 / (light.attenuation.x + 
        light.attenuation.y * distance + 
        light.attenuation.z * (distance * distance));

    return attenuation * (diffuse_color + specular_color);
}

float CalculateShadow(vec3 world_pos) {
    if (!use_shadow_mapping) {
        return 1.0;
    }
    
    vec4 light_ndc = world_to_light_ndc_matrix * vec4(world_pos, 1.0);
    vec3 light_coords = light_ndc.xyz * 0.5 + 0.5;
    
    // Check if outside shadow map bounds
    if (light_coords.x < 0.0 || light_coords.x > 1.0 ||
        light_coords.y < 0.0 || light_coords.y > 1.0 ||
        light_coords.z < 0.0 || light_coords.z > 1.0) {
        return 1.0;
    }
    
    float current_depth = light_coords.z;
    float bias = 0.005;
    
    // PCF: Sample a 3x3 grid around the current texel
    float shadow = 0.0;
    vec2 texel_size = 1.0 / vec2(4096.0, 4096.0);  // Shadow map resolution
    
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 offset = vec2(float(x), float(y)) * texel_size;
            float depth_from_light = texture(shadow_map, light_coords.xy + offset).r;
            shadow += (current_depth - bias) > depth_from_light ? 0.0 : 1.0;
        }
    }
    
    shadow /= 9.0;  // Average of 9 samples
    
    return shadow;
}

vec3 CalcDirectionalLight(vec3 normal, vec3 view_dir) {
    float shadow = CalculateShadow(world_position);  // Use world_position, not frag_world_pos

    DirectionalLight light = directional_light;
    vec3 light_dir = normalize(-light.direction);
    float diffuse_intensity = max(dot(normal, light_dir), 0.0);
    vec3 diffuse_color = diffuse_intensity * light.diffuse * GetDiffuseColor() * shadow;
    
    vec3 reflect_dir = reflect(-light_dir, normal);
    float specular_intensity = pow(
        max(dot(view_dir, reflect_dir), 0.0), material.shininess);
    vec3 specular_color = specular_intensity * 
        light.specular * GetSpecularColor() * shadow;

    return diffuse_color + specular_color;
}