fn sqrt_aa0d7a() {
  var arg_0 = vec4<f32>();
  var res : vec4<f32> = sqrt(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  sqrt_aa0d7a();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  sqrt_aa0d7a();
}

@compute @workgroup_size(1)
fn compute_main() {
  sqrt_aa0d7a();
}
