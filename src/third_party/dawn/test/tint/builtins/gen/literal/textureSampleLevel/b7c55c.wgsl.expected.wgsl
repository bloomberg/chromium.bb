@group(1) @binding(0) var arg_0 : texture_2d_array<f32>;

@group(1) @binding(1) var arg_1 : sampler;

fn textureSampleLevel_b7c55c() {
  var res : vec4<f32> = textureSampleLevel(arg_0, arg_1, vec2<f32>(), 1, 1.0, vec2<i32>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureSampleLevel_b7c55c();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureSampleLevel_b7c55c();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureSampleLevel_b7c55c();
}
