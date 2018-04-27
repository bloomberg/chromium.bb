#ifndef THIRD_PARTY_RNNOISE_SRC_RNN_VAD_WEIGHTS_H_
#define THIRD_PARTY_RNNOISE_SRC_RNN_VAD_WEIGHTS_H_

#include <cstdint>

namespace rnnoise {

// Weights scaling factor.
extern const float kWeightsScale = 1.f / 256.f;

// Input layer (dense).
extern const size_t kInputLayerOutputSize = 24;
extern const int8_t kInputDenseWeights[1008];
extern const int8_t kInputDenseBias[24];

// Hidden layer (GRU).
extern const size_t kHiddenLayerOutputSize = 24;
extern const int8_t kHiddenGruWeights[1728];
extern const int8_t kHiddenGruRecurrentWeights[1728];
extern const int8_t kHiddenGruBias[72];

// Output layer (dense).
extern const size_t kOutputLayerOutputSize = 1;
extern const int8_t kOutputDenseWeights[24];
extern const int8_t kOutputDenseBias[1];

}  // namespace rnnoise

#endif  // THIRD_PARTY_RNNOISE_SRC_RNN_VAD_WEIGHTS_H_
