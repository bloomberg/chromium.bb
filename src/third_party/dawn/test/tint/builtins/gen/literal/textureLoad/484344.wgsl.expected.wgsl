@group(1) @binding(0) var arg_0 : texture_2d<f32>;

fn textureLoad_484344() {
  var res : vec4<f32> = textureLoad(arg_0, vec2<i32>(), 0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureLoad_484344();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureLoad_484344();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureLoad_484344();
}
