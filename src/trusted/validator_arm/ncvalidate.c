#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/validator_x86/ncvalidate.h"

struct NCValidatorState *NCValidateInit(const uint32_t vbase,
                                        const uint32_t vlimit,
                                        const uint8_t alignment) {
  /* BUG(petr): not implemented */
  return NULL;
}

void NCValidateSegment(uint8_t *mbase, uint32_t vbase, size_t sz,
                       struct NCValidatorState *vstate) {
  /* BUG(petr): not implemented */
}

int NCValidateFinish(struct NCValidatorState *vstate) {
  /* BUG(petr): not implemented */
  return 0;
}

void NCValidateFreeState(struct NCValidatorState **vstate) {
  /* BUG(petr): not implemented */
}

int modify_ldt(int func, void* ptr, uint32_t bytecount) {
  /* BUG(petr): not implemented */
  return 0;
}

