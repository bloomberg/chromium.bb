SKIP: FAILED

#version 310 es

uniform highp sampler2DArrayShadow arg_0_arg_1;

void textureSampleCompareLevel_b6e47c() {
  float res = textureOffset(arg_0_arg_1, vec4(0.0f, 0.0f, float(1), 1.0f), ivec2(0));
}

vec4 vertex_main() {
  textureSampleCompareLevel_b6e47c();
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
ERROR: 0:6: 'sampler' : TextureOffset does not support sampler2DArrayShadow :  ES Profile
ERROR: 0:6: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



#version 310 es
precision mediump float;

uniform highp sampler2DArrayShadow arg_0_arg_1;

void textureSampleCompareLevel_b6e47c() {
  float res = textureOffset(arg_0_arg_1, vec4(0.0f, 0.0f, float(1), 1.0f), ivec2(0));
}

void fragment_main() {
  textureSampleCompareLevel_b6e47c();
}

void main() {
  fragment_main();
  return;
}
Error parsing GLSL shader:
ERROR: 0:7: 'sampler' : TextureOffset does not support sampler2DArrayShadow :  ES Profile
ERROR: 0:7: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



#version 310 es

uniform highp sampler2DArrayShadow arg_0_arg_1;

void textureSampleCompareLevel_b6e47c() {
  float res = textureOffset(arg_0_arg_1, vec4(0.0f, 0.0f, float(1), 1.0f), ivec2(0));
}

void compute_main() {
  textureSampleCompareLevel_b6e47c();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
Error parsing GLSL shader:
ERROR: 0:6: 'sampler' : TextureOffset does not support sampler2DArrayShadow :  ES Profile
ERROR: 0:6: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



