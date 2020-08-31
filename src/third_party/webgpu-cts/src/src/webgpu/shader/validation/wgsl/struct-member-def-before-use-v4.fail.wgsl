# v-0006 - Fails because struct `foo` does not have a member `c`however `f.c` is
# used.

type foo = struct {
  [[offset 0]] var b : f32;
  [[offset 8]] var a : array<f32>;
}

fn main() -> void {
  var f : foo;
  f.c = 2;
  return;
}
entry_point vertex = main;
