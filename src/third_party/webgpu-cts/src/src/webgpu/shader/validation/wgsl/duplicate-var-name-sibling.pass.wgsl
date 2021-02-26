# v-0014 - This passes because variable `a` is redeclared in a sibling scope.

[[stage(vertex)]]
fn main() -> void {
  if (true) {
    var a : i32 = -1;
  }
  if (true) {
    var a : i32 = -2;
  }
  return;
}

