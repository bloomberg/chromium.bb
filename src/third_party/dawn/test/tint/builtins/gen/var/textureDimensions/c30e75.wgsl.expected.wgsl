@group(1) @binding(0) var arg_0 : texture_storage_2d<r32sint, write>;

fn textureDimensions_c30e75() {
  var res : vec2<i32> = textureDimensions(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureDimensions_c30e75();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureDimensions_c30e75();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureDimensions_c30e75();
}
