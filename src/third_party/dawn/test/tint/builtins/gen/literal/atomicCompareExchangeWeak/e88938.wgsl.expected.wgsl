var<workgroup> arg_0 : atomic<i32>;

fn atomicCompareExchangeWeak_e88938() {
  var res = atomicCompareExchangeWeak(&(arg_0), 1, 1);
}

@compute @workgroup_size(1)
fn compute_main() {
  atomicCompareExchangeWeak_e88938();
}
