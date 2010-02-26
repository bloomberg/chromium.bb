attribute vec3 pos;
attribute vec3 normal;
attribute vec4 colorIn;

varying vec4 color;

uniform mat4 mvp;
uniform mat3 normalMatrix;
uniform vec4 ambient;
uniform float shininess;
uniform vec3 light_0_direction;
uniform vec4 light_0_diffuse;
uniform vec4 light_0_specular;
uniform vec3 light_1_direction;
uniform vec4 light_1_diffuse;
uniform vec3 light_2_direction;
uniform vec4 light_2_diffuse;

vec3 worldNormal;

vec4 SpecularLight(vec3 direction,
                   vec4 diffuseColor,
                   vec4 specularColor) {
  vec3 lightDir = normalize(direction);
  float diffuse = max(0., dot(worldNormal, lightDir));
  float specular = 0.;
  if (diffuse > 0.) {
    vec3 halfv = normalize(lightDir + vec3(0., 0., 1.));
    specular = pow(max(0., dot(halfv, worldNormal)), shininess);
  }
  return diffuse * diffuseColor * colorIn + specular * specularColor;
}

vec4 DiffuseLight(vec3 direction, vec4 diffuseColor) {
  vec3 lightDir = normalize(direction);
  float diffuse = max(0., dot(worldNormal, lightDir));
  return diffuse * diffuseColor * colorIn;
}

void main() {
  worldNormal = normalize(normalMatrix * normal);

  gl_Position = mvp * vec4(pos, 1.);

  color = ambient * colorIn;
  color += SpecularLight(light_0_direction, light_0_diffuse,
                         light_0_specular);
  color += DiffuseLight(light_1_direction, light_1_diffuse);
  color += DiffuseLight(light_2_direction, light_2_diffuse);
}
