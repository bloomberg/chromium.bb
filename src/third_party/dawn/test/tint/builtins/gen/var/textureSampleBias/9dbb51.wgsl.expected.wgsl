@group(1) @binding(0) var arg_0 : texture_2d_array<f32>;

@group(1) @binding(1) var arg_1 : sampler;

fn textureSampleBias_9dbb51() {
  var arg_2 = vec2<f32>();
  var arg_3 = 1;
  var arg_4 = 1.0;
  var res : vec4<f32> = textureSampleBias(arg_0, arg_1, arg_2, arg_3, arg_4, vec2<i32>());
}

@fragment
fn fragment_main() {
  textureSampleBias_9dbb51();
}
