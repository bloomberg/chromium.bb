# v-0016 - This fails because of the duplicate function `my_func`.

fn my_func() -> void {
  return;
}

fn my_func() -> void {
  return;
} 

[[stage(vertex)]]
fn main() -> void {
  return;
}

