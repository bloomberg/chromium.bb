# v-0004 - Self recursion is dis-allowed and `other` calls itself.

[[stage(vertex)]]
fn other() -> i32 {
  return other();
}
