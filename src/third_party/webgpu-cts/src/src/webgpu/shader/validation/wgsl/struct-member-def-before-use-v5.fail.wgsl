# v-0006 - Fails because struct `foo` does not have a member `b`however `f.b` is
# used.

type goo = struct {
  b : f32;
}

type foo = struct {
  a : f32;
}

fn main() -> void {
  var f : foo;
  f.a = 2.0;
  f.b = 5.0;
  return;
}
entry_point vertex = main;
