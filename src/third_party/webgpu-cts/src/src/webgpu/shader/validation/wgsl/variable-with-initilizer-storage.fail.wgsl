// v-0032: variable 'u' has an initializer, however its storage class is 'storage'.

[[block]]
struct PositionBuffer {
  [[offset(0)]] pos: vec2<f32>;
};

[[group(0), binding(0)]]
var<storage> s : PositionBuffer = PositionBuffer(vec2<f32>(0.0, 0.0));

[[stage(vertex)]]
fn main() -> void {
}
