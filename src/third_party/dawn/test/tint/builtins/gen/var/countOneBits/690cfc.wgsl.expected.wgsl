fn countOneBits_690cfc() {
  var arg_0 = vec3<u32>();
  var res : vec3<u32> = countOneBits(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  countOneBits_690cfc();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  countOneBits_690cfc();
}

@compute @workgroup_size(1)
fn compute_main() {
  countOneBits_690cfc();
}
