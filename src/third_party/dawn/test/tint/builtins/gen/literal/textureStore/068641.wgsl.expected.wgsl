@group(1) @binding(0) var arg_0 : texture_storage_3d<rgba16uint, write>;

fn textureStore_068641() {
  textureStore(arg_0, vec3<i32>(), vec4<u32>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureStore_068641();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureStore_068641();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureStore_068641();
}
