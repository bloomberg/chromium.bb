#version 310 es

void reflect_05357e() {
  vec4 arg_0 = vec4(0.0f);
  vec4 arg_1 = vec4(0.0f);
  vec4 res = reflect(arg_0, arg_1);
}

vec4 vertex_main() {
  reflect_05357e();
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

void reflect_05357e() {
  vec4 arg_0 = vec4(0.0f);
  vec4 arg_1 = vec4(0.0f);
  vec4 res = reflect(arg_0, arg_1);
}

void fragment_main() {
  reflect_05357e();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void reflect_05357e() {
  vec4 arg_0 = vec4(0.0f);
  vec4 arg_1 = vec4(0.0f);
  vec4 res = reflect(arg_0, arg_1);
}

void compute_main() {
  reflect_05357e();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
