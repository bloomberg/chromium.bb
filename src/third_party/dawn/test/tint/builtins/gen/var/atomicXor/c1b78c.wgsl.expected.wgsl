struct SB_RW {
  arg_0 : atomic<i32>,
}

@group(0) @binding(0) var<storage, read_write> sb_rw : SB_RW;

fn atomicXor_c1b78c() {
  var arg_1 = 1;
  var res : i32 = atomicXor(&(sb_rw.arg_0), arg_1);
}

@fragment
fn fragment_main() {
  atomicXor_c1b78c();
}

@compute @workgroup_size(1)
fn compute_main() {
  atomicXor_c1b78c();
}
