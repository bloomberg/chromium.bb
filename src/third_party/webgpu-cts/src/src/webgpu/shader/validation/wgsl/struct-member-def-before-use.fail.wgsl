# v-0006 - Fails because 'f' is not a struct.

[[stage(vertex)]]
fn main() -> void {
  var f : f32;
  f.a = 4.0;
  return;
}
