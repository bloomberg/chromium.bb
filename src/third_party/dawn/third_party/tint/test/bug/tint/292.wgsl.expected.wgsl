[[stage(vertex)]]
fn main() -> [[builtin(position)]] vec4<f32> {
  var light : vec3<f32> = vec3<f32>(1.200000048, 1.0, 2.0);
  var negative_light : vec3<f32> = -(light);
  return vec4<f32>();
}
