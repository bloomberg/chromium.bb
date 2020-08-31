# v-0006 -  This fails because `fn Foo()` returns `struct goo`, which does not 
# have a member `s.z`.

type goo = struct {
  s : vec2<i32>;
}

fn Foo() -> type goo {
  var a : type goo;
  a.s.x = 2;
  a.s.y = 3;
  return a;
}

fn main() -> void {
  var r : i32 = Foo().s.z;
  return;
}
entry_point fragment = main;
