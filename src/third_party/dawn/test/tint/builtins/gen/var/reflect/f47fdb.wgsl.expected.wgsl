fn reflect_f47fdb() {
  var arg_0 = vec3<f32>();
  var arg_1 = vec3<f32>();
  var res : vec3<f32> = reflect(arg_0, arg_1);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  reflect_f47fdb();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  reflect_f47fdb();
}

@compute @workgroup_size(1)
fn compute_main() {
  reflect_f47fdb();
}
