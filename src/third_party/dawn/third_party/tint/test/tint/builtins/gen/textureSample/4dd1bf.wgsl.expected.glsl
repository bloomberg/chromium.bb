SKIP: FAILED

#version 310 es
precision mediump float;

uniform highp samplerCubeArray arg_0_arg_1;

void textureSample_4dd1bf() {
  vec4 res = texture(arg_0_arg_1, vec4(0.0f, 0.0f, 0.0f, float(1)));
}

void fragment_main() {
  textureSample_4dd1bf();
}

void main() {
  fragment_main();
  return;
}
Error parsing GLSL shader:
ERROR: 0:4: 'samplerCubeArray' : Reserved word. 
ERROR: 0:4: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



