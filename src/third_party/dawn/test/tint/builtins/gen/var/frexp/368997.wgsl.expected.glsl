#version 310 es

struct frexp_result_vec3 {
  vec3 sig;
  ivec3 exp;
};

frexp_result_vec3 tint_frexp(vec3 param_0) {
  frexp_result_vec3 result;
  result.sig = frexp(param_0, result.exp);
  return result;
}


void frexp_368997() {
  vec3 arg_0 = vec3(0.0f);
  frexp_result_vec3 res = tint_frexp(arg_0);
}

vec4 vertex_main() {
  frexp_368997();
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

struct frexp_result_vec3 {
  vec3 sig;
  ivec3 exp;
};

frexp_result_vec3 tint_frexp(vec3 param_0) {
  frexp_result_vec3 result;
  result.sig = frexp(param_0, result.exp);
  return result;
}


void frexp_368997() {
  vec3 arg_0 = vec3(0.0f);
  frexp_result_vec3 res = tint_frexp(arg_0);
}

void fragment_main() {
  frexp_368997();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

struct frexp_result_vec3 {
  vec3 sig;
  ivec3 exp;
};

frexp_result_vec3 tint_frexp(vec3 param_0) {
  frexp_result_vec3 result;
  result.sig = frexp(param_0, result.exp);
  return result;
}


void frexp_368997() {
  vec3 arg_0 = vec3(0.0f);
  frexp_result_vec3 res = tint_frexp(arg_0);
}

void compute_main() {
  frexp_368997();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
