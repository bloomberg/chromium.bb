@group(1) @binding(0) var arg_0 : texture_storage_2d_array<rgba32uint, write>;

fn textureStore_8e0479() {
  textureStore(arg_0, vec2<i32>(), 1, vec4<u32>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureStore_8e0479();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureStore_8e0479();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureStore_8e0479();
}
