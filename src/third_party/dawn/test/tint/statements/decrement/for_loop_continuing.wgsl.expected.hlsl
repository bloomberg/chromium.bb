[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

RWByteAddressBuffer i : register(u0, space0);

void main() {
  {
    [loop] for(; (i.Load(0u) < 10u); i.Store(0u, asuint((i.Load(0u) - 1u)))) {
    }
  }
}
