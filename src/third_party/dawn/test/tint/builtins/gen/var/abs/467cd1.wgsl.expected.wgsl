fn abs_467cd1() {
  var arg_0 = 1u;
  var res : u32 = abs(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  abs_467cd1();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  abs_467cd1();
}

@compute @workgroup_size(1)
fn compute_main() {
  abs_467cd1();
}
