# v-0040: variable 'u' is in uniform storage class so it must be declared with group and binding
# decoration. 

[[block]]
struct Params {
  [[offset(0)]] count: i32;
};

var<uniform> u : Params;

[[stage(vertex)]]
fn main() -> void {
}
