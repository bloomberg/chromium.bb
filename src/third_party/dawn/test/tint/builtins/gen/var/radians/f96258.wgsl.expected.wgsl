fn radians_f96258() {
  var arg_0 = vec3<f32>();
  var res : vec3<f32> = radians(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  radians_f96258();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  radians_f96258();
}

@compute @workgroup_size(1)
fn compute_main() {
  radians_f96258();
}
