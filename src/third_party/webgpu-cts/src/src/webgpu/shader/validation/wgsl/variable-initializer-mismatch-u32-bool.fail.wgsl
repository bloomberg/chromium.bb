// v-0033: variable 'u' store type is 'u32' however the initializer type is 'bool'.

var<out> u : u32  = true;

[[stage(vertex)]]
fn main() -> void {
}
