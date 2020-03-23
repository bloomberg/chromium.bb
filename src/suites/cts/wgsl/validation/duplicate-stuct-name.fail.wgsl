# v-0012 - This fails because of the duplicated `foo` structure.

type foo = struct {}

type foo = struct {
  [[offset 0]] b : f32;
}

fn main() -> void {
  return;
}
entry_point vertex = main;

