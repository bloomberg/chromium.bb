[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

RWByteAddressBuffer v : register(u0, space0);
static uint i = 0u;

int idx1() {
  i = (i + 1u);
  return 1;
}

int idx2() {
  i = (i + 2u);
  return 1;
}

int idx3() {
  i = (i + 3u);
  return 1;
}

void foo() {
  float a[4] = (float[4])0;
  const int tint_symbol_2 = idx1();
  const int tint_symbol_save = tint_symbol_2;
  {
    a[tint_symbol_save] = (a[tint_symbol_save] * 2.0f);
    [loop] while (true) {
      const int tint_symbol_3 = idx2();
      if (!((a[tint_symbol_3] < 10.0f))) {
        break;
      }
      {
      }
      {
        const int tint_symbol_4 = idx3();
        const int tint_symbol_1_save = tint_symbol_4;
        a[tint_symbol_1_save] = (a[tint_symbol_1_save] + 1.0f);
      }
    }
  }
}
