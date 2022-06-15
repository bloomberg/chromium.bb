struct SB_RW {
  arg_0 : atomic<i32>,
}

@group(0) @binding(0) var<storage, read_write> sb_rw : SB_RW;

fn atomicExchange_f2e22f() {
  var arg_1 = 1;
  var res : i32 = atomicExchange(&(sb_rw.arg_0), arg_1);
}

@fragment
fn fragment_main() {
  atomicExchange_f2e22f();
}

@compute @workgroup_size(1)
fn compute_main() {
  atomicExchange_f2e22f();
}
