# v-0008 - This fails because the case does not end with `break`, `fallthrough`,
# or `return`

fn main() -> void {
  switch(5) {
    case 1: {
      fallthrough;
    }
    default: {
       a = 3;
    }
  }
  return;
}
entry_point vertex = main;

