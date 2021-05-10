typedef struct S1 {
  int x;
} T1;

struct S3 {
  int x;
};

typedef struct {
  int x;
  struct S3 s3;
} T4;

typedef union U5 {
  int x;
  double y;
} T5;

typedef struct S6 {
  struct {
    int x;
  };
  union {
    int y;
    int z;
  };
} T6;

typedef struct S7 {
  struct {
    int x;
  } y;
  union {
    int w;
  } z;
} T7;

int main() {}
