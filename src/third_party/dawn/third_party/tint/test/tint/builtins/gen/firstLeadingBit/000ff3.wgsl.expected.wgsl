fn firstLeadingBit_000ff3() {
  var res : vec4<u32> = firstLeadingBit(vec4<u32>());
}

@stage(vertex)
fn vertex_main() -> @builtin(position) vec4<f32> {
  firstLeadingBit_000ff3();
  return vec4<f32>();
}

@stage(fragment)
fn fragment_main() {
  firstLeadingBit_000ff3();
}

@stage(compute) @workgroup_size(1)
fn compute_main() {
  firstLeadingBit_000ff3();
}
