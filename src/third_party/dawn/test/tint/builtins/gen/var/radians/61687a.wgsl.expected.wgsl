fn radians_61687a() {
  var arg_0 = vec2<f32>();
  var res : vec2<f32> = radians(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  radians_61687a();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  radians_61687a();
}

@compute @workgroup_size(1)
fn compute_main() {
  radians_61687a();
}
