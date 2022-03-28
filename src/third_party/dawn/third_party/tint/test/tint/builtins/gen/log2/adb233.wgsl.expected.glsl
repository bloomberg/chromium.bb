#version 310 es

void log2_adb233() {
  vec3 res = log2(vec3(0.0f, 0.0f, 0.0f));
}

vec4 vertex_main() {
  log2_adb233();
  return vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

void main() {
  vec4 inner_result = vertex_main();
  gl_Position = inner_result;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  return;
}
#version 310 es
precision mediump float;

void log2_adb233() {
  vec3 res = log2(vec3(0.0f, 0.0f, 0.0f));
}

void fragment_main() {
  log2_adb233();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void log2_adb233() {
  vec3 res = log2(vec3(0.0f, 0.0f, 0.0f));
}

void compute_main() {
  log2_adb233();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
