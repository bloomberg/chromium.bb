ByteAddressBuffer tint_symbol : register(t0, space0);
RWByteAddressBuffer tint_symbol_1 : register(u1, space0);

[numthreads(1, 1, 1)]
void main() {
  tint_symbol_1.Store(0u, asuint(tint_symbol.Load(0u)));
  return;
}
