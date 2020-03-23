# v-0001 - This test fails as only "GLSL.std.450" is permitted in as import
# statement.

import "Other" as other;

fn main() -> void {
  return;
}
entry_point vertex = main;
