// Modified from shader at
// http://nehe.gamedev.net/data/articles/article.asp?article=21

uniform mat4 g_ModelViewProjectionMatrix;

attribute vec3 g_Vertex;
attribute vec4 g_MultiTexCoord0;

varying vec2 texture_coordinate;

void main()
{
  // Transforming The Vertex
  gl_Position = g_ModelViewProjectionMatrix * g_Vertex;

  // Passing The Texture Coordinate Of Texture Unit 0 To The Fragment Shader
  texture_coordinate = vec2(g_MultiTexCoord0);
}
