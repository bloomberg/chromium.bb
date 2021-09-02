// v-0032 - This fails because the runtime array does not have a stride attribute.

[[block]]
struct Foo {
  [[offset (0)]] a : array<f32>;
};

[[stage(vertex)]]
fn main() {
  return;
}
