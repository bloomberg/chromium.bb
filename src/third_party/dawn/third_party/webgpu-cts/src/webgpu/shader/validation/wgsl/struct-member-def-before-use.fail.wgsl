// v-0007 - Fails because 'f' is not a struct.

@stage(fragment)
fn main() {
  var f : f32;
  f.a = 4.0;
  return;
}
