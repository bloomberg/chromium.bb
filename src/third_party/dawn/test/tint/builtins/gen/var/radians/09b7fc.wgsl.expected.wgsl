fn radians_09b7fc() {
  var arg_0 = vec4<f32>();
  var res : vec4<f32> = radians(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  radians_09b7fc();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  radians_09b7fc();
}

@compute @workgroup_size(1)
fn compute_main() {
  radians_09b7fc();
}
