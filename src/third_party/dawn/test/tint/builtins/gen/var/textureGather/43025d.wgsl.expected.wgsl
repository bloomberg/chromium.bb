@group(1) @binding(0) var arg_0 : texture_depth_cube_array;

@group(1) @binding(1) var arg_1 : sampler;

fn textureGather_43025d() {
  var arg_2 = vec3<f32>();
  var arg_3 = 1;
  var res : vec4<f32> = textureGather(arg_0, arg_1, arg_2, arg_3);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  textureGather_43025d();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  textureGather_43025d();
}

@compute @workgroup_size(1)
fn compute_main() {
  textureGather_43025d();
}
