@group(1) @binding(0) var arg_0 : texture_storage_3d<rgba8uint, write>;

fn textureStore_f8dead() {
  var arg_1 = vec3<i32>();
  var arg_2 = vec4<u32>();
  textureStore(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureStore_f8dead();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureStore_f8dead();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureStore_f8dead();
}
