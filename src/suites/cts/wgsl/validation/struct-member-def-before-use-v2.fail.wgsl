# v-0006 - Fails because struct 'boo' does not have a member 't'.

type boo = struct {
  z : f32;
}

type goo = struct {
  y : boo;
}

type foo = struct {
  x : goo;
}

fn main() -> void {
  var f : foo;
  f.g = 1;
  f.x.y.t = 2.0;
  return;
}
entry_point vertex = main;
