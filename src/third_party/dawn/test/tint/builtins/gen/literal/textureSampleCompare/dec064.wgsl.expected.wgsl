@group(1) @binding(0) var arg_0 : texture_depth_2d;

@group(1) @binding(1) var arg_1 : sampler_comparison;

fn textureSampleCompare_dec064() {
  var res : f32 = textureSampleCompare(arg_0, arg_1, vec2<f32>(), 1.0, vec2<i32>());
}

@fragment
fn fragment_main() {
  textureSampleCompare_dec064();
}
