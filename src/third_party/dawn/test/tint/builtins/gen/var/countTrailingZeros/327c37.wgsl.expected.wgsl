fn countTrailingZeros_327c37() {
  var arg_0 = vec2<i32>();
  var res : vec2<i32> = countTrailingZeros(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  countTrailingZeros_327c37();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  countTrailingZeros_327c37();
}

@compute @workgroup_size(1)
fn compute_main() {
  countTrailingZeros_327c37();
}
