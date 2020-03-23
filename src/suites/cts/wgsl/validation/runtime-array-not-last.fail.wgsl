# v-0015 - This fails because of the runtime array in the struct.

type Foo = struct {
  [[offset 0]] a : array<f32>;
  [[offset 8]] b : f32;
}

fn main() -> void {
  return;
}
entry_point vertex = main;

