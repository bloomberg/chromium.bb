@group(1) @binding(0) var arg_0 : texture_depth_2d;

@group(1) @binding(1) var arg_1 : sampler_comparison;

fn textureSampleCompareLevel_2ad2b1() {
  var arg_2 = vec2<f32>();
  var arg_3 = 1.0;
  var res : f32 = textureSampleCompareLevel(arg_0, arg_1, arg_2, arg_3);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureSampleCompareLevel_2ad2b1();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureSampleCompareLevel_2ad2b1();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureSampleCompareLevel_2ad2b1();
}
