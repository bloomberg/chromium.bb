fn log_3da25a() {
  var arg_0 = vec4<f32>();
  var res : vec4<f32> = log(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  log_3da25a();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  log_3da25a();
}

@compute @workgroup_size(1)
fn compute_main() {
  log_3da25a();
}
