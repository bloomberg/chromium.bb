@group(1) @binding(1) var arg_1 : texture_2d_array<f32>;

@group(1) @binding(2) var arg_2 : sampler;

fn textureGather_22e930() {
  var res : vec4<f32> = textureGather(1, arg_1, arg_2, vec2<f32>(), 1);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureGather_22e930();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureGather_22e930();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureGather_22e930();
}
