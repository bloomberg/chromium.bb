@group(1) @binding(0) var arg_0 : texture_depth_cube_array;

@group(1) @binding(1) var arg_1 : sampler;

fn textureSampleLevel_ae5e39() {
  var arg_2 = vec3<f32>();
  var arg_3 = 1;
  var arg_4 = 0;
  var res : f32 = textureSampleLevel(arg_0, arg_1, arg_2, arg_3, arg_4);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureSampleLevel_ae5e39();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureSampleLevel_ae5e39();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureSampleLevel_ae5e39();
}
