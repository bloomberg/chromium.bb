fn smoothstep_6c4975() {
  var arg_0 = 1.0;
  var arg_1 = 1.0;
  var arg_2 = 1.0;
  var res : f32 = smoothstep(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  smoothstep_6c4975();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  smoothstep_6c4975();
}

@compute @workgroup_size(1)
fn compute_main() {
  smoothstep_6c4975();
}
