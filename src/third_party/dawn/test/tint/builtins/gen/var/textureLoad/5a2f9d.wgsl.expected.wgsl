@group(1) @binding(0) var arg_0 : texture_1d<i32>;

fn textureLoad_5a2f9d() {
  var arg_1 = 1;
  var arg_2 = 0;
  var res : vec4<i32> = textureLoad(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureLoad_5a2f9d();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureLoad_5a2f9d();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureLoad_5a2f9d();
}
