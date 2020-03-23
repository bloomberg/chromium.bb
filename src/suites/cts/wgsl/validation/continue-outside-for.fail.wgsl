# v-0010 - This fails because the continue is used outside of a for block.

fn main() -> void {
  continue;
  return;
}
entry_point vertex = main;
