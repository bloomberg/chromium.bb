typedef struct RD {
  int u;
  int v;
  int arr[3];
} RD;

typedef struct VP9_COMP {
  int y;
  RD *rd;
  RD rd2;
  RD rd3[2];
} VP9_COMP;

int parse_lvalue_2(VP9_COMP *cpi) { RD *rd2 = &cpi->rd2; }

int func(VP9_COMP *cpi, int x) {
  cpi->rd->u = 0;

  int y;
  y = 0;

  cpi->rd2.v = 0;

  cpi->rd->arr[2] = 0;

  cpi->rd3[1]->arr[2] = 0;

  return 0;
}

int main() {
  int x = 0;
  VP9_COMP cpi;
  func(&cpi, x);
}
