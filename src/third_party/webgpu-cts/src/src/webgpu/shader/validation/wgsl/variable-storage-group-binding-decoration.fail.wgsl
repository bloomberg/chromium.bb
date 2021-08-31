# v-0039: variable 's' is in storage storage class so it must be declared with group and binding
# decoration.

[[block]]
struct PositionBuffer {
  [[offset(0)]] pos: vec2<f32>; 
};

var<storage> s : PositionBuffer;

[[stage(vertex)]]
fn main() {
}
