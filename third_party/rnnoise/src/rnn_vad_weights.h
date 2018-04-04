#ifndef THIRD_PARTY_RNNOISE_SRC_RNN_VAD_WEIGHTS_H_
#define THIRD_PARTY_RNNOISE_SRC_RNN_VAD_WEIGHTS_H_

namespace rnnoise {

extern const int8_t kInputDenseWeights[1008];
extern const int8_t kInputDenseBias[24];
extern const int8_t kHiddenGruWeights[1728];
extern const int8_t kHiddenGruRecurrentWeights[1728];
extern const int8_t kHiddenGruBias[72];
extern const int8_t kOutputWeights[24];
extern const int8_t kOutputBias[1];

}  // namespace rnnoise

#endif  // THIRD_PARTY_RNNOISE_SRC_RNN_VAD_WEIGHTS_H_
