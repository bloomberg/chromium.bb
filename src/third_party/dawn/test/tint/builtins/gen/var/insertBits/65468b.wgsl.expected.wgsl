fn insertBits_65468b() {
  var arg_0 = 1;
  var arg_1 = 1;
  var arg_2 = 1u;
  var arg_3 = 1u;
  var res : i32 = insertBits(arg_0, arg_1, arg_2, arg_3);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  insertBits_65468b();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  insertBits_65468b();
}

@compute @workgroup_size(1)
fn compute_main() {
  insertBits_65468b();
}
