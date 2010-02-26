const float bumpHeight = 0.2;

uniform sampler2D normalSampler;
uniform samplerCube envSampler;

varying vec2 texCoord;
varying vec3 worldEyeVec;
varying vec3 worldNormal;
varying vec3 worldTangent;
varying vec3 worldBinorm;

void main() {
  vec2 bump = (texture2D(normalSampler, texCoord.xy).xy * 2.0 - 1.0) * bumpHeight;
  vec3 normal = normalize(worldNormal);
  vec3 tangent = normalize(worldTangent);
  vec3 binormal = normalize(worldBinorm);
  vec3 nb = normal + bump.x * tangent + bump.y * binormal;
  nb = normalize(nb);
  vec3 worldEye = normalize(worldEyeVec);
  vec3 lookup = reflect(worldEye, nb);
  vec4 color = textureCube(envSampler, lookup);
  gl_FragColor = color;
}
