fn min_82b28f() {
  var arg_0 = vec2<u32>();
  var arg_1 = vec2<u32>();
  var res : vec2<u32> = min(arg_0, arg_1);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  min_82b28f();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  min_82b28f();
}

@compute @workgroup_size(1)
fn compute_main() {
  min_82b28f();
}
