// v-0033: variable 'a' store type is 'i32' however the initializer type is 'bool'.

var<out> a : i32  = true;

[[stage(vertex)]]
fn main() -> void {
}
