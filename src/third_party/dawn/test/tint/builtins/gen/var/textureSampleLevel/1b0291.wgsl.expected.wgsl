@group(1) @binding(0) var arg_0 : texture_depth_cube;

@group(1) @binding(1) var arg_1 : sampler;

fn textureSampleLevel_1b0291() {
  var arg_2 = vec3<f32>();
  var arg_3 = 0;
  var res : f32 = textureSampleLevel(arg_0, arg_1, arg_2, arg_3);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureSampleLevel_1b0291();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureSampleLevel_1b0291();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureSampleLevel_1b0291();
}
