fn countLeadingZeros_70783f() {
  var arg_0 = vec2<u32>();
  var res : vec2<u32> = countLeadingZeros(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  countLeadingZeros_70783f();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  countLeadingZeros_70783f();
}

@compute @workgroup_size(1)
fn compute_main() {
  countLeadingZeros_70783f();
}
