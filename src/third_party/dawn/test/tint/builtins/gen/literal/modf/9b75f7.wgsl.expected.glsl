#version 310 es

struct modf_result_vec3 {
  vec3 fract;
  vec3 whole;
};

modf_result_vec3 tint_modf(vec3 param_0) {
  modf_result_vec3 result;
  result.fract = modf(param_0, result.whole);
  return result;
}


void modf_9b75f7() {
  modf_result_vec3 res = tint_modf(vec3(0.0f));
}

vec4 vertex_main() {
  modf_9b75f7();
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

struct modf_result_vec3 {
  vec3 fract;
  vec3 whole;
};

modf_result_vec3 tint_modf(vec3 param_0) {
  modf_result_vec3 result;
  result.fract = modf(param_0, result.whole);
  return result;
}


void modf_9b75f7() {
  modf_result_vec3 res = tint_modf(vec3(0.0f));
}

void fragment_main() {
  modf_9b75f7();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

struct modf_result_vec3 {
  vec3 fract;
  vec3 whole;
};

modf_result_vec3 tint_modf(vec3 param_0) {
  modf_result_vec3 result;
  result.fract = modf(param_0, result.whole);
  return result;
}


void modf_9b75f7() {
  modf_result_vec3 res = tint_modf(vec3(0.0f));
}

void compute_main() {
  modf_9b75f7();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
