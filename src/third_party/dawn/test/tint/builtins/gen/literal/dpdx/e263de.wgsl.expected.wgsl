fn dpdx_e263de() {
  var res : f32 = dpdx(1.0);
}

@fragment
fn fragment_main() {
  dpdx_e263de();
}
