var<workgroup> v : i32;

@compute @workgroup_size(1)
fn main() {
    _ = v;
}
