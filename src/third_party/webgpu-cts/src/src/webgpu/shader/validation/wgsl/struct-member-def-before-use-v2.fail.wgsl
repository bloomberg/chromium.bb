// v-0007 - Fails because struct 'boo' does not have a member 't'.

struct boo {
  z : f32;
};

struct goo {
  y : boo;
};

struct foo {
  x : goo;
};

[[stage(vertex)]]
fn main() {
  var f : foo;
  f.x.y.t = 2.0;
  return;
}
