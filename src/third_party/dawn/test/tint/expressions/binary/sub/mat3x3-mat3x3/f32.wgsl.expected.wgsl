@compute @workgroup_size(1)
fn f() {
  let a = mat3x3<f32>(vec3<f32>(1.0, 2.0, 3.0), vec3<f32>(4.0, 5.0, 6.0), vec3<f32>(7.0, 8.0, 9.0));
  let b = mat3x3<f32>(vec3<f32>(-1.0, -2.0, -3.0), vec3<f32>(-4.0, -5.0, -6.0), vec3<f32>(-7.0, -8.0, -9.0));
  let r : mat3x3<f32> = (a - b);
}
