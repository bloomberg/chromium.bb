attribute vec3 g_Position;
attribute vec3 g_TexCoord0;
attribute vec3 g_Tangent;
attribute vec3 g_Binormal;
attribute vec3 g_Normal;

uniform mat4 world;
uniform mat4 worldInverseTranspose;
uniform mat4 worldViewProj;
uniform mat4 viewInverse;

varying vec2 texCoord;
varying vec3 worldEyeVec;
varying vec3 worldNormal;
varying vec3 worldTangent;
varying vec3 worldBinorm;

void main() {
  gl_Position = worldViewProj * vec4(g_Position.xyz, 1.);
  texCoord.xy = g_TexCoord0.xy;
  worldNormal = (worldInverseTranspose * vec4(g_Normal, 1.)).xyz;
  worldTangent = (worldInverseTranspose * vec4(g_Tangent, 1.)).xyz;
  worldBinorm = (worldInverseTranspose * vec4(g_Binormal, 1.)).xyz;
  vec3 worldPos = (world * vec4(g_Position, 1.)).xyz;
  worldEyeVec = normalize(worldPos - viewInverse[3].xyz);
}
