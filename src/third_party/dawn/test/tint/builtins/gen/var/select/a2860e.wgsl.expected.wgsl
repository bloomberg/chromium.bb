fn select_a2860e() {
  var arg_0 = vec4<i32>();
  var arg_1 = vec4<i32>();
  var arg_2 = vec4<bool>();
  var res : vec4<i32> = select(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  select_a2860e();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  select_a2860e();
}

@compute @workgroup_size(1)
fn compute_main() {
  select_a2860e();
}
