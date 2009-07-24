// Modified from shader at
// http://nehe.gamedev.net/data/articles/article.asp?article=21

uniform mat4 g_ModelViewProjectionMatrix;

attribute vec3 g_Vertex;

void main()
{
  gl_Position = g_ModelViewProjectionMatrix * g_Vertex;
}
