@group(1) @binding(0) var arg_0 : texture_depth_cube_array;

fn textureNumLevels_2c3575() {
  var res : i32 = textureNumLevels(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureNumLevels_2c3575();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureNumLevels_2c3575();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureNumLevels_2c3575();
}
