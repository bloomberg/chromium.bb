SKIP: FAILED

#version 310 es

layout(rgba8i) uniform highp writeonly iimage1D arg_0;
void textureStore_bf775c() {
  int arg_1 = 1;
  ivec4 arg_2 = ivec4(0);
  imageStore(arg_0, arg_1, arg_2);
}

vec4 vertex_main() {
  textureStore_bf775c();
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
Error parsing GLSL shader:
ERROR: 0:3: 'iimage1D' : Reserved word. 
WARNING: 0:3: 'layout' : useless application of layout qualifier 
ERROR: 0:3: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



#version 310 es
precision mediump float;

layout(rgba8i) uniform highp writeonly iimage1D arg_0;
void textureStore_bf775c() {
  int arg_1 = 1;
  ivec4 arg_2 = ivec4(0);
  imageStore(arg_0, arg_1, arg_2);
}

void fragment_main() {
  textureStore_bf775c();
}

void main() {
  fragment_main();
  return;
}
Error parsing GLSL shader:
ERROR: 0:4: 'iimage1D' : Reserved word. 
WARNING: 0:4: 'layout' : useless application of layout qualifier 
ERROR: 0:4: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



#version 310 es

layout(rgba8i) uniform highp writeonly iimage1D arg_0;
void textureStore_bf775c() {
  int arg_1 = 1;
  ivec4 arg_2 = ivec4(0);
  imageStore(arg_0, arg_1, arg_2);
}

void compute_main() {
  textureStore_bf775c();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
Error parsing GLSL shader:
ERROR: 0:3: 'iimage1D' : Reserved word. 
WARNING: 0:3: 'layout' : useless application of layout qualifier 
ERROR: 0:3: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



