[[group(1), binding(0)]] var arg_0 : texture_1d<f32>;

[[group(1), binding(1)]] var arg_1 : sampler;

fn textureSample_6e64fb() {
  var res : vec4<f32> = textureSample(arg_0, arg_1, 1.0);
}

[[stage(fragment)]]
fn fragment_main() {
  textureSample_6e64fb();
}
