fn refract_cd905f() {
  var res : vec2<f32> = refract(vec2<f32>(), vec2<f32>(), 1.0);
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
