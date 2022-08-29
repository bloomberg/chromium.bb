#version 310 es

layout(r32ui) uniform highp writeonly uimage3D arg_0;
void textureStore_ef9f2f() {
  ivec3 arg_1 = ivec3(0);
  uvec4 arg_2 = uvec4(0u);
  imageStore(arg_0, arg_1, arg_2);
}

vec4 vertex_main() {
  textureStore_ef9f2f();
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

layout(r32ui) uniform highp writeonly uimage3D arg_0;
void textureStore_ef9f2f() {
  ivec3 arg_1 = ivec3(0);
  uvec4 arg_2 = uvec4(0u);
  imageStore(arg_0, arg_1, arg_2);
}

void fragment_main() {
  textureStore_ef9f2f();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

layout(r32ui) uniform highp writeonly uimage3D arg_0;
void textureStore_ef9f2f() {
  ivec3 arg_1 = ivec3(0);
  uvec4 arg_2 = uvec4(0u);
  imageStore(arg_0, arg_1, arg_2);
}

void compute_main() {
  textureStore_ef9f2f();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
