@group(1) @binding(0) var arg_0 : texture_multisampled_2d<f32>;

fn textureLoad_a583c9() {
  var arg_1 = vec2<i32>();
  var arg_2 = 1;
  var res : vec4<f32> = textureLoad(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureLoad_a583c9();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureLoad_a583c9();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureLoad_a583c9();
}
