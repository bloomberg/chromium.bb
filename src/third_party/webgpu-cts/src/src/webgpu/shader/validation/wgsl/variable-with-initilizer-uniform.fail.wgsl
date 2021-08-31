// v-0032: variable 'u' has an initializer, however its storage class is 'uniform'.

[[block]]
struct Params {
  [[offset(0)]] count: i32;
};

[[group(0), binding(0)]]
var<uniform> u : Params = Params(1);

[[stage(vertex)]]
fn main() {
}
