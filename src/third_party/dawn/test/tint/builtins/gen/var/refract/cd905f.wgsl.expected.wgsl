fn refract_cd905f() {
  var arg_0 = vec2<f32>();
  var arg_1 = vec2<f32>();
  var arg_2 = 1.0;
  var res : vec2<f32> = refract(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  refract_cd905f();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  refract_cd905f();
}

@compute @workgroup_size(1)
fn compute_main() {
  refract_cd905f();
}
