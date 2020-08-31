# v-0014 - This fails because of the duplicated `a` variable.

fn main() -> void {
  if (true) {
    var a : u32;
  }
  if (false) {
    var a : i32;
  }
  return;
}
entry_point vertex = main;

