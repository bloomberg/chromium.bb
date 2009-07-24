// Modified from shader at
// http://nehe.gamedev.net/data/articles/article.asp?article=21

uniform mat4 g_ModelViewProjectionMatrix;
uniform mat4 g_ModelViewMatrix;
uniform mat4 g_NormalMatrix;
uniform vec4 g_LightSource0Position;

attribute vec3 g_Vertex;
attribute vec3 g_Normal;

varying vec3 normal;
varying vec3 vertex_to_light_vector;

void main()
{
  // Transforming The Vertex
  gl_Position = g_ModelViewProjectionMatrix * g_Vertex;

  // Transforming The Normal To ModelView-Space
  normal = g_NormalMatrix * g_Normal;

  // Transforming The Vertex Position To ModelView-Space
  vec4 vertex_in_modelview_space = g_ModelViewMatrix * g_Vertex;

  // Calculating The Vector From The Vertex Position To The Light Position
  vertex_to_light_vector = vec3(g_LightSource0Position - vertex_in_modelview_space);
}
