@group(1) @binding(0) var arg_0 : texture_storage_2d_array<r32float, write>;

fn textureNumLayers_d5b228() {
  var res : i32 = textureNumLayers(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureNumLayers_d5b228();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureNumLayers_d5b228();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureNumLayers_d5b228();
}
