fn select_3c25ce() {
  var arg_0 = vec3<bool>();
  var arg_1 = vec3<bool>();
  var arg_2 = bool();
  var res : vec3<bool> = select(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  select_3c25ce();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  select_3c25ce();
}

@compute @workgroup_size(1)
fn compute_main() {
  select_3c25ce();
}
