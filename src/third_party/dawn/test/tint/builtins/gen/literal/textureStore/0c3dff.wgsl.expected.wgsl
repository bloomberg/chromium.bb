@group(1) @binding(0) var arg_0 : texture_storage_2d<rgba16uint, write>;

fn textureStore_0c3dff() {
  textureStore(arg_0, vec2<i32>(), vec4<u32>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureStore_0c3dff();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureStore_0c3dff();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureStore_0c3dff();
}
