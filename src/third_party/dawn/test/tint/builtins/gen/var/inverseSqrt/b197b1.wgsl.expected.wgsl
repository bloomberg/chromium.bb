fn inverseSqrt_b197b1() {
  var arg_0 = vec3<f32>();
  var res : vec3<f32> = inverseSqrt(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  inverseSqrt_b197b1();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  inverseSqrt_b197b1();
}

@compute @workgroup_size(1)
fn compute_main() {
  inverseSqrt_b197b1();
}
