@group(1) @binding(0) var arg_0 : texture_depth_2d;

@group(1) @binding(1) var arg_1 : sampler;

fn textureSampleLevel_02be59() {
  var arg_2 = vec2<f32>();
  var arg_3 = 0;
  var res : f32 = textureSampleLevel(arg_0, arg_1, arg_2, arg_3);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureSampleLevel_02be59();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureSampleLevel_02be59();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureSampleLevel_02be59();
}
