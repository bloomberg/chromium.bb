fn dpdxCoarse_f64d7b() {
  var arg_0 = vec3<f32>();
  var res : vec3<f32> = dpdxCoarse(arg_0);
}

@fragment
fn fragment_main() {
  dpdxCoarse_f64d7b();
}
