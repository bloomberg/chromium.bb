@group(1) @binding(0) var arg_0 : texture_2d<f32>;

@group(1) @binding(1) var arg_1 : sampler;

fn textureSample_51b514() {
  var arg_2 = vec2<f32>();
  var res : vec4<f32> = textureSample(arg_0, arg_1, arg_2);
}

@fragment
fn fragment_main() {
  textureSample_51b514();
}
