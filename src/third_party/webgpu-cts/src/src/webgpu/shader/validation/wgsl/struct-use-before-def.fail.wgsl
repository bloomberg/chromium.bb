# v-0007 - This fails because the structure is used before definition.

const a : Foo;

struct Foo {
  [[offset 0]] a : i32;
};

[[stage(vertex)]]
fn main() -> void {
  return;
}
