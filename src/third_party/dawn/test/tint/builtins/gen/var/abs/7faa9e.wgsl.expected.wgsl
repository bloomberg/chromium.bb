fn abs_7faa9e() {
  var arg_0 = vec2<i32>();
  var res : vec2<i32> = abs(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  abs_7faa9e();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  abs_7faa9e();
}

@compute @workgroup_size(1)
fn compute_main() {
  abs_7faa9e();
}
