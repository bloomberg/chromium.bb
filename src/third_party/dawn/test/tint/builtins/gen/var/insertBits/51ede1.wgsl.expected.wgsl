fn insertBits_51ede1() {
  var arg_0 = vec4<u32>();
  var arg_1 = vec4<u32>();
  var arg_2 = 1u;
  var arg_3 = 1u;
  var res : vec4<u32> = insertBits(arg_0, arg_1, arg_2, arg_3);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  insertBits_51ede1();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  insertBits_51ede1();
}

@compute @workgroup_size(1)
fn compute_main() {
  insertBits_51ede1();
}
