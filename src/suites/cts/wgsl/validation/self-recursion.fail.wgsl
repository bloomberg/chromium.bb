# v-0004 - Self recursion is dis-allowed and `other` calls itself.

fn other() -> i32 {
  return other();
}

fn main() -> void {
  return;
}
entry_point vertex = main;

