SKIP: FAILED

#version 310 es

uniform highp isamplerCubeArray arg_0_1;
void textureNumLayers_85f980() {
  int res = textureSize(arg_0_1, 0).z;
}

vec4 vertex_main() {
  textureNumLayers_85f980();
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
ERROR: 0:3: 'isamplerCubeArray' : Reserved word. 
ERROR: 0:3: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



#version 310 es
precision mediump float;

uniform highp isamplerCubeArray arg_0_1;
void textureNumLayers_85f980() {
  int res = textureSize(arg_0_1, 0).z;
}

void fragment_main() {
  textureNumLayers_85f980();
}

void main() {
  fragment_main();
  return;
}
Error parsing GLSL shader:
ERROR: 0:4: 'isamplerCubeArray' : Reserved word. 
ERROR: 0:4: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



#version 310 es

uniform highp isamplerCubeArray arg_0_1;
void textureNumLayers_85f980() {
  int res = textureSize(arg_0_1, 0).z;
}

void compute_main() {
  textureNumLayers_85f980();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
Error parsing GLSL shader:
ERROR: 0:3: 'isamplerCubeArray' : Reserved word. 
ERROR: 0:3: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



