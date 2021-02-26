# v-0015 - This fails because of the runtime array is not last member of the struct.

struct Foo {
  [[offset (0)]] a : array<f32>;
  [[offset (8)]] b : f32;
};

[[stage(vertex)]]
fn main() -> void {
  return;
}
