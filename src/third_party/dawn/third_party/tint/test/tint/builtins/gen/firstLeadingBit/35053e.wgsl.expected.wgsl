fn firstLeadingBit_35053e() {
  var res : vec3<i32> = firstLeadingBit(vec3<i32>());
}

@stage(vertex)
fn vertex_main() -> @builtin(position) vec4<f32> {
  firstLeadingBit_35053e();
  return vec4<f32>();
}

@stage(fragment)
fn fragment_main() {
  firstLeadingBit_35053e();
}

@stage(compute) @workgroup_size(1)
fn compute_main() {
  firstLeadingBit_35053e();
}
