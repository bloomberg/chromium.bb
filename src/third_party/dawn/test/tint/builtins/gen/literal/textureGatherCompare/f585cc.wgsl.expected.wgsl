@group(1) @binding(0) var arg_0 : texture_depth_2d_array;

@group(1) @binding(1) var arg_1 : sampler_comparison;

fn textureGatherCompare_f585cc() {
  var res : vec4<f32> = textureGatherCompare(arg_0, arg_1, vec2<f32>(), 1, 1.0, vec2<i32>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureGatherCompare_f585cc();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureGatherCompare_f585cc();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureGatherCompare_f585cc();
}
