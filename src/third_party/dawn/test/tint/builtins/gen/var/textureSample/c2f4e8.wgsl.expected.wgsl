@group(1) @binding(0) var arg_0 : texture_depth_cube_array;

@group(1) @binding(1) var arg_1 : sampler;

fn textureSample_c2f4e8() {
  var arg_2 = vec3<f32>();
  var arg_3 = 1;
  var res : f32 = textureSample(arg_0, arg_1, arg_2, arg_3);
}

@fragment
fn fragment_main() {
  textureSample_c2f4e8();
}
