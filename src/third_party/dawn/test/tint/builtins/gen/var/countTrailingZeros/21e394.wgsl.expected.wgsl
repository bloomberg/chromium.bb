fn countTrailingZeros_21e394() {
  var arg_0 = 1u;
  var res : u32 = countTrailingZeros(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  countTrailingZeros_21e394();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  countTrailingZeros_21e394();
}

@compute @workgroup_size(1)
fn compute_main() {
  countTrailingZeros_21e394();
}
