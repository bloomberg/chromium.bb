fn countOneBits_af90e2() {
  var arg_0 = vec2<i32>();
  var res : vec2<i32> = countOneBits(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  countOneBits_af90e2();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  countOneBits_af90e2();
}

@compute @workgroup_size(1)
fn compute_main() {
  countOneBits_af90e2();
}
