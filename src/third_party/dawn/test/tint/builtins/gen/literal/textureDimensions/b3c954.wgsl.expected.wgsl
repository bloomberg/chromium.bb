@group(1) @binding(0) var arg_0 : texture_cube<u32>;

fn textureDimensions_b3c954() {
  var res : vec2<i32> = textureDimensions(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureDimensions_b3c954();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureDimensions_b3c954();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureDimensions_b3c954();
}
