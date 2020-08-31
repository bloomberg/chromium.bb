# v-0021 - Fails because `c` is a const and can not be re-assigned.

fn main() -> void {
  const c : f32 = 0.1;
  c = 0.2;
  return;
}
entry_point vertex = main;
