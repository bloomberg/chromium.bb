fn pack2x16snorm_6c169b() {
  var arg_0 = vec2<f32>();
  var res : u32 = pack2x16snorm(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  pack2x16snorm_6c169b();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  pack2x16snorm_6c169b();
}

@compute @workgroup_size(1)
fn compute_main() {
  pack2x16snorm_6c169b();
}
