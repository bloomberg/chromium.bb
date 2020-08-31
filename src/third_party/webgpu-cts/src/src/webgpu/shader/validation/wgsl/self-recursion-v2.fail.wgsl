# v-0004 - Self recursion is dis-allowed and `c` calls itself.


fn d() -> i32 {
  return 2;
}

fn c() -> i32 {
  return c() + d();
}

fn b() -> i32 {
  return c();
}

fn a() -> i32 {
  return b();
}

fn main() -> void {
  var v : i32 = a();
  return;
}
entry_point vertex = main;

