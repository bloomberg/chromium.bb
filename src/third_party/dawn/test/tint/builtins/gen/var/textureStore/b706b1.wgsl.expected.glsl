#version 310 es

layout(rgba8i) uniform highp writeonly iimage3D arg_0;
void textureStore_b706b1() {
  ivec3 arg_1 = ivec3(0);
  ivec4 arg_2 = ivec4(0);
  imageStore(arg_0, arg_1, arg_2);
}

vec4 vertex_main() {
  textureStore_b706b1();
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

layout(rgba8i) uniform highp writeonly iimage3D arg_0;
void textureStore_b706b1() {
  ivec3 arg_1 = ivec3(0);
  ivec4 arg_2 = ivec4(0);
  imageStore(arg_0, arg_1, arg_2);
}

void fragment_main() {
  textureStore_b706b1();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

layout(rgba8i) uniform highp writeonly iimage3D arg_0;
void textureStore_b706b1() {
  ivec3 arg_1 = ivec3(0);
  ivec4 arg_2 = ivec4(0);
  imageStore(arg_0, arg_1, arg_2);
}

void compute_main() {
  textureStore_b706b1();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
