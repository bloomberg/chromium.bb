// Modified from shader at
// http://nehe.gamedev.net/data/articles/article.asp?article=21

uniform vec4 g_Color;

void main()
{
  // Set each pixel to a constant color
  gl_FragColor = g_Color;
}
