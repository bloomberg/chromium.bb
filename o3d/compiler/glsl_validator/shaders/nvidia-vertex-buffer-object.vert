// Per-vertex phong shader
uniform mat4 worldViewProjection;
uniform vec3 lightWorldPos;
uniform vec4 lightColor;

uniform mat4 world;
uniform mat4 viewInverse;
uniform mat4 worldInverseTranspose;

uniform vec4 emissiveColor;
uniform vec4 ambientColor;
uniform vec4 diffuseColor;
uniform vec4 specularColor;
uniform float shininess;
uniform float specularFactor;

attribute vec3 g_Position;
attribute vec3 g_Normal;

varying vec4 v_color;

vec4 lit(float n_dot_l, float n_dot_h, float m) {
  return vec4(1.,
              clamp(n_dot_l, 0., 1.),
              // FIXME: approximation to
              // (n_dot_l < 0) || (n_dot_h < 0)
              pow(clamp(n_dot_h, 0., 1.), m),
              1.);
}

void main() {
  vec4 position = vec4(g_Position, 1.);
  vec4 worldPosition = world * position;
  vec3 normal = normalize((worldInverseTranspose *
                           vec4(g_Normal, 0.)).xyz);
  vec3 surfaceToLight = normalize(lightWorldPos - worldPosition.xyz);
  vec3 surfaceToView = normalize((viewInverse[3] - worldPosition).xyz);
  vec3 halfVector = normalize(surfaceToLight + surfaceToView);
  vec4 litR = lit(dot(normal, surfaceToLight),
                  dot(normal, halfVector), shininess);
  v_color =
      vec4((emissiveColor +
            lightColor * (ambientColor * litR.x +
                          diffuseColor * litR.y +
                          specularColor * litR.z * specularFactor)).rgb,
           diffuseColor.a);
  gl_Position = worldViewProjection * position;
}
