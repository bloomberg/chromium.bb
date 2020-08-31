# v-0005 - Fails because `a` calls `c` but `c` has not been defined.

fn a() -> i32 {
  return c();
}

fn b() -> i32 {
  return a();
}

fn c() -> i32 {
  return a();
}

fn main() -> void {
  return;
}
entry_point vertex = main;

