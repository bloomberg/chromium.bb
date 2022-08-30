#version 310 es

void determinant_2b62ba() {
  mat3 arg_0 = mat3(vec3(0.0f), vec3(0.0f), vec3(0.0f));
  float res = determinant(arg_0);
}

vec4 vertex_main() {
  determinant_2b62ba();
  return vec4(0.0f);
}

void main() {
  gl_PointSize = 1.0;
  vec4 inner_result = vertex_main();
  gl_Position = inner_result;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  return;
}
#version 310 es
precision mediump float;

void determinant_2b62ba() {
  mat3 arg_0 = mat3(vec3(0.0f), vec3(0.0f), vec3(0.0f));
  float res = determinant(arg_0);
}

void fragment_main() {
  determinant_2b62ba();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void determinant_2b62ba() {
  mat3 arg_0 = mat3(vec3(0.0f), vec3(0.0f), vec3(0.0f));
  float res = determinant(arg_0);
}

void compute_main() {
  determinant_2b62ba();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
