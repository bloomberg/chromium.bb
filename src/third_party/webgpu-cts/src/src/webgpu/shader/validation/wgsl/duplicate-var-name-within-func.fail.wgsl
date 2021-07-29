// v-0014 - This fails because variable `a` is redeclared.

[[stage(fragment)]]
fn main() {
  var a : u32 = 1u;
  if (true) {
    var a : i32 = -1;
  }
  return;
}

