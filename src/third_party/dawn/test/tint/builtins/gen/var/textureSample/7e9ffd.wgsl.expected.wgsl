@group(1) @binding(0) var arg_0 : texture_depth_2d_array;

@group(1) @binding(1) var arg_1 : sampler;

fn textureSample_7e9ffd() {
  var arg_2 = vec2<f32>();
  var arg_3 = 1;
  var res : f32 = textureSample(arg_0, arg_1, arg_2, arg_3);
}

@fragment
fn fragment_main() {
  textureSample_7e9ffd();
}
