// pass v-0033: variable 'a' and its initializer have the same storetype, 'f32'.

var<out> a : f32  = 0.0;

[[stage(vertex)]]
fn main() -> void {
}
