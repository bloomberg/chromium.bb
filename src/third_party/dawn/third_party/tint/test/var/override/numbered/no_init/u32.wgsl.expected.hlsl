SKIP: FAILED

#ifndef WGSL_SPEC_CONSTANT_1234
#error spec constant required for constant id 1234
#endif
static const uint o = WGSL_SPEC_CONSTANT_1234;

[numthreads(1, 1, 1)]
void main() {
  return;
}
/tmp/tint_U0EZFZ:2:2: error: spec constant required for constant id 1234
#error spec constant required for constant id 1234
 ^


