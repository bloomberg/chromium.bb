// Modified from shader at
// http://nehe.gamedev.net/data/articles/article.asp?article=21

uniform sampler2D my_color_texture;

varying vec2 texture_coordinate;

void main()
{
  // Sampling The Texture And Passing It To The Frame Buffer
  gl_FragColor = texture2D(my_color_texture, texture_coordinate);
}
