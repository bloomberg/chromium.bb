[[group(1), binding(0)]] var arg_0 : texture_depth_cube_array;

[[group(1), binding(1)]] var arg_1 : sampler_comparison;

fn textureSampleCompare_a3ca7e() {
  var res : f32 = textureSampleCompare(arg_0, arg_1, vec3<f32>(), 1, 1.0);
}

[[stage(fragment)]]
fn fragment_main() {
  textureSampleCompare_a3ca7e();
}
