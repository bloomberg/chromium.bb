@group(1) @binding(0) var arg_0 : texture_storage_1d<rgba32float, write>;

fn textureStore_6b75c3() {
  textureStore(arg_0, 1, vec4<f32>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureStore_6b75c3();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureStore_6b75c3();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureStore_6b75c3();
}
