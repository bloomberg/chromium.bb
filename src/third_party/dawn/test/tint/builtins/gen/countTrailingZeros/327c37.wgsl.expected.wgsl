fn countTrailingZeros_327c37() {
  var res : vec2<i32> = countTrailingZeros(vec2<i32>());
}

@stage(vertex)
fn vertex_main() -> @builtin(position) vec4<f32> {
  countTrailingZeros_327c37();
  return vec4<f32>();
}

@stage(fragment)
fn fragment_main() {
  countTrailingZeros_327c37();
}

@stage(compute) @workgroup_size(1)
fn compute_main() {
  countTrailingZeros_327c37();
}
