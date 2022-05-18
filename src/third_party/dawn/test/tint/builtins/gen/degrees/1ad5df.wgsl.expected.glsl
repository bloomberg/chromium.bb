#version 310 es

vec2 tint_degrees(vec2 param_0) {
  return param_0 * 57.295779513082322865;
}


void degrees_1ad5df() {
  vec2 res = tint_degrees(vec2(0.0f, 0.0f));
}

vec4 vertex_main() {
  degrees_1ad5df();
  return vec4(0.0f, 0.0f, 0.0f, 0.0f);
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

vec2 tint_degrees(vec2 param_0) {
  return param_0 * 57.295779513082322865;
}


void degrees_1ad5df() {
  vec2 res = tint_degrees(vec2(0.0f, 0.0f));
}

void fragment_main() {
  degrees_1ad5df();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

vec2 tint_degrees(vec2 param_0) {
  return param_0 * 57.295779513082322865;
}


void degrees_1ad5df() {
  vec2 res = tint_degrees(vec2(0.0f, 0.0f));
}

void compute_main() {
  degrees_1ad5df();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
