@group(1) @binding(0) var arg_0 : texture_2d_array<u32>;

fn textureDimensions_1f20c5() {
  var res : vec2<i32> = textureDimensions(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureDimensions_1f20c5();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureDimensions_1f20c5();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureDimensions_1f20c5();
}
