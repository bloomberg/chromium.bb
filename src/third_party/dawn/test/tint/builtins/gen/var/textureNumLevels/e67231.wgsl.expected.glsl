SKIP: FAILED

#version 310 es

uniform highp sampler2D arg_0_1;
void textureNumLevels_e67231() {
  int res = textureQueryLevels(arg_0_1);
}

vec4 vertex_main() {
  textureNumLevels_e67231();
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
ERROR: 0:5: 'textureQueryLevels' : no matching overloaded function found 
ERROR: 0:5: '=' :  cannot convert from ' const float' to ' temp highp int'
ERROR: 0:5: '' : compilation terminated 
ERROR: 3 compilation errors.  No code generated.



#version 310 es
precision mediump float;

uniform highp sampler2D arg_0_1;
void textureNumLevels_e67231() {
  int res = textureQueryLevels(arg_0_1);
}

void fragment_main() {
  textureNumLevels_e67231();
}

void main() {
  fragment_main();
  return;
}
Error parsing GLSL shader:
ERROR: 0:6: 'textureQueryLevels' : no matching overloaded function found 
ERROR: 0:6: '=' :  cannot convert from ' const float' to ' temp mediump int'
ERROR: 0:6: '' : compilation terminated 
ERROR: 3 compilation errors.  No code generated.



#version 310 es

uniform highp sampler2D arg_0_1;
void textureNumLevels_e67231() {
  int res = textureQueryLevels(arg_0_1);
}

void compute_main() {
  textureNumLevels_e67231();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
Error parsing GLSL shader:
ERROR: 0:5: 'textureQueryLevels' : no matching overloaded function found 
ERROR: 0:5: '=' :  cannot convert from ' const float' to ' temp highp int'
ERROR: 0:5: '' : compilation terminated 
ERROR: 3 compilation errors.  No code generated.



