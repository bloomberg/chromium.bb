@group(1) @binding(0) var arg_0 : texture_1d<f32>;

fn textureDimensions_b3e407() {
  var arg_1 = 0;
  var res : i32 = textureDimensions(arg_0, arg_1);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureDimensions_b3e407();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureDimensions_b3e407();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureDimensions_b3e407();
}
