fn abs_5ad50a() {
  var res : vec3<i32> = abs(vec3<i32>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  abs_5ad50a();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  abs_5ad50a();
}

@compute @workgroup_size(1)
fn compute_main() {
  abs_5ad50a();
}
