# v-0016 - This fails because of the duplicate function `my_func`.

fn my_func() -> void {}
fn my_func() -> void {}

fn main() -> void {
  return;
}
entry_point vertex = main;

