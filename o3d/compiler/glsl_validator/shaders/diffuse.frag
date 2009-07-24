// Modified from shader at
// http://nehe.gamedev.net/data/articles/article.asp?article=21

varying vec3 normal;
varying vec3 vertex_to_light_vector;

void main()
{
  // Defining The Material Colors
  const vec4 AMBIENT_COLOR = vec4(0.1, 0.0, 0.0, 1.0);
  const vec4 DIFFUSE_COLOR = vec4(1.0, 0.0, 0.0, 1.0);

  // Scaling The Input Vector To Length 1
  vec3 normalized_normal = normalize(normal);
  vec3 normalized_vertex_to_light_vector = normalize(vertex_to_light_vector);

  // Calculating The Diffuse Term And Clamping It To [0;1]
  float diffuseTerm = clamp(dot(normal, vertex_to_light_vector), 0.0, 1.0);

  // Calculating The Final Color
  gl_FragColor = AMBIENT_COLOR + DIFFUSE_COLOR * diffuseTerm;
}
