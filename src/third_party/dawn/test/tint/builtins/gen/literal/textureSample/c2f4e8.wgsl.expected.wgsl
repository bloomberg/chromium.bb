@group(1) @binding(0) var arg_0 : texture_depth_cube_array;

@group(1) @binding(1) var arg_1 : sampler;

fn textureSample_c2f4e8() {
  var res : f32 = textureSample(arg_0, arg_1, vec3<f32>(), 1);
}

@fragment
fn fragment_main() {
  textureSample_c2f4e8();
}
