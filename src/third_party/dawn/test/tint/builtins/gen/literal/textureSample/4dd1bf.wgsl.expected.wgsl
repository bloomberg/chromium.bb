@group(1) @binding(0) var arg_0 : texture_cube_array<f32>;

@group(1) @binding(1) var arg_1 : sampler;

fn textureSample_4dd1bf() {
  var res : vec4<f32> = textureSample(arg_0, arg_1, vec3<f32>(), 1);
}

@fragment
fn fragment_main() {
  textureSample_4dd1bf();
}
