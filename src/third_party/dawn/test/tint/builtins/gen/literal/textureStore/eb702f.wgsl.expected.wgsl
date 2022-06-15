@group(1) @binding(0) var arg_0 : texture_storage_3d<r32float, write>;

fn textureStore_eb702f() {
  textureStore(arg_0, vec3<i32>(), vec4<f32>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureStore_eb702f();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureStore_eb702f();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureStore_eb702f();
}
