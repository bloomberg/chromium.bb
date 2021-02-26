# v-0012 - This fails because |struct Foo|, |fn Foo| and |var Foo| have
# the same name |Foo|.

struct Foo {
  b : f32;
};

fn Foo() ->void {
  return;
}

[[stage(vertex)]]
fn main() -> void {
  var Foo : f32;
  var f : Foo;
  return;
}
