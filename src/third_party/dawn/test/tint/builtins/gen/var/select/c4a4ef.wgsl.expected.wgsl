fn select_c4a4ef() {
  var arg_0 = vec4<u32>();
  var arg_1 = vec4<u32>();
  var arg_2 = vec4<bool>();
  var res : vec4<u32> = select(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  select_c4a4ef();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  select_c4a4ef();
}

@compute @workgroup_size(1)
fn compute_main() {
  select_c4a4ef();
}
