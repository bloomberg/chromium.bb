fn dot_7548a0() {
  var arg_0 = vec3<u32>();
  var arg_1 = vec3<u32>();
  var res : u32 = dot(arg_0, arg_1);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  dot_7548a0();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  dot_7548a0();
}

@compute @workgroup_size(1)
fn compute_main() {
  dot_7548a0();
}
