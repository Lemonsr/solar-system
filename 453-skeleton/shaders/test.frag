#version 330 core

in vec3 fragPos;
in vec2 tc;
in vec3 n;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform sampler2D sampler;

out vec4 color;

void main() {
	vec4 d = texture(sampler, tc);
	vec3 lightColor = vec3(1.0);
	vec3 lightDir = normalize(fragPos - lightPos);
    vec3 normal = normalize(n);

    float diff = max(dot(lightDir, normal), 0.0);
	vec3 diffuse = diff * lightColor;

	float specularStrength = 0.8;
	vec3 viewDir = normalize(viewPos - fragPos);
	vec3 reflectDir = reflect(-lightDir, normal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
	vec3 specular = specularStrength * spec * lightColor;

	float ambientStrength = 0.05;
    vec3 ambient = ambientStrength * lightColor;

	color = vec4((diffuse + specular + ambient), 1.0) * d;
}
