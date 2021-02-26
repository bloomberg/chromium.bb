# v-0006 - Fails because struct `foo` does not have a member `c`however `f.c`
# is used.

struct foo {
  [[offset (0)]] b : f32;
  [[offset (8)]] a : array<f32>;
};

[[stage(vertex)]]
fn main() -> void {
  var f : foo;
  f.c = 2;
  return;
}
