# v-0009 - This fails because the break is used outside a for or switch block.

fn main() -> void {
  if (true) {
    break;
  }
  return;
}
entry_point vertex = main;

