fn select_51b047() {
  var arg_0 = vec2<u32>();
  var arg_1 = vec2<u32>();
  var arg_2 = bool();
  var res : vec2<u32> = select(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  select_51b047();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  select_51b047();
}

@compute @workgroup_size(1)
fn compute_main() {
  select_51b047();
}
