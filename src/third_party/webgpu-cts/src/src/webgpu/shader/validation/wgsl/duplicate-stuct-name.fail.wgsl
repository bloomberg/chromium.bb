# v-0012 - This fails because of the duplicated `foo` structure.

struct foo {
  [[offset (0)]] a : i32;
};

struct foo {
  [[offset (0)]] b : f32;
};

[[stage(vertex)]]
fn main() -> void {
  return;
}

