# v-0009 - This fails because the break is used outside a for or switch block.

[[stage(vertex)]]
fn main() -> void {
  if (true) {
    break;
  }
  return;
}

