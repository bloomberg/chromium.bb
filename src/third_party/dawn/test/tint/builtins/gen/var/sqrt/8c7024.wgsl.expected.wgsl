fn sqrt_8c7024() {
  var arg_0 = vec2<f32>();
  var res : vec2<f32> = sqrt(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  sqrt_8c7024();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  sqrt_8c7024();
}

@compute @workgroup_size(1)
fn compute_main() {
  sqrt_8c7024();
}
