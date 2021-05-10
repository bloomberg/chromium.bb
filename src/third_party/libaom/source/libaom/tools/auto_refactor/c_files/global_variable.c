extern const int global_a[13];

const int global_b = 0;

typedef struct S1 {
  int x;
} T1;

struct S3 {
  int x;
} s3;

int func_global_1(int *a) {
  *a = global_a[3];
  return 0;
}
