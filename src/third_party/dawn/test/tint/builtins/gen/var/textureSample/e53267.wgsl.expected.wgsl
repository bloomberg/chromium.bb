@group(1) @binding(0) var arg_0 : texture_cube<f32>;

@group(1) @binding(1) var arg_1 : sampler;

fn textureSample_e53267() {
  var arg_2 = vec3<f32>();
  var res : vec4<f32> = textureSample(arg_0, arg_1, arg_2);
}

@fragment
fn fragment_main() {
  textureSample_e53267();
}
