@group(1) @binding(0) var arg_0 : texture_storage_1d<r32uint, write>;

fn textureStore_102722() {
  var arg_1 = 1;
  var arg_2 = vec4<u32>();
  textureStore(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureStore_102722();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureStore_102722();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureStore_102722();
}
