fn pack2x16float_0e97b3() {
  var arg_0 = vec2<f32>();
  var res : u32 = pack2x16float(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  pack2x16float_0e97b3();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  pack2x16float_0e97b3();
}

@compute @workgroup_size(1)
fn compute_main() {
  pack2x16float_0e97b3();
}
