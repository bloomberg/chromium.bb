struct SB_RW {
  arg_0 : atomic<u32>,
}

@group(0) @binding(0) var<storage, read_write> sb_rw : SB_RW;

fn atomicAnd_85a8d9() {
  var arg_1 = 1u;
  var res : u32 = atomicAnd(&(sb_rw.arg_0), arg_1);
}

@fragment
fn fragment_main() {
  atomicAnd_85a8d9();
}

@compute @workgroup_size(1)
fn compute_main() {
  atomicAnd_85a8d9();
}
