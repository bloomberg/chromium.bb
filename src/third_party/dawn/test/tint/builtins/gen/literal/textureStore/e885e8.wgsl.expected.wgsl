@group(1) @binding(0) var arg_0 : texture_storage_1d<rgba16float, write>;

fn textureStore_e885e8() {
  textureStore(arg_0, 1, vec4<f32>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureStore_e885e8();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureStore_e885e8();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureStore_e885e8();
}
