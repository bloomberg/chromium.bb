fn main_1() {
  let x_24 : f32 = mat3x3<f32>(vec3<f32>(1.0, 2.0, 3.0), vec3<f32>(4.0, 5.0, 6.0), vec3<f32>(7.0, 8.0, 9.0))[1u].y;
  return;
}

[[stage(compute), workgroup_size(1, 1, 1)]]
fn main() {
  main_1();
}
