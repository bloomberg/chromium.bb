// v-0033: variable 'f' store type is 'f32' however the initializer type is 'bool'.

var<out> f : f32  = true;

[[stage(vertex)]]
fn main() -> void {
}
