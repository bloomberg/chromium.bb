// Copyright 2019 The libgav1 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/gav1/decoder.h"

#include "src/decoder_impl.h"

namespace libgav1 {

Decoder::Decoder() = default;

Decoder::~Decoder() = default;

StatusCode Decoder::Init(const DecoderSettings* const settings) {
  if (initialized_) return kLibgav1StatusAlready;
  if (settings != nullptr) settings_ = *settings;
  const StatusCode status = DecoderImpl::Create(&settings_, &impl_);
  if (status != kLibgav1StatusOk) return status;
  initialized_ = true;
  return kLibgav1StatusOk;
}

StatusCode Decoder::EnqueueFrame(const uint8_t* data, const size_t size,
                                 int64_t user_private_data) {
  if (!initialized_) return kLibgav1StatusNotInitialized;
  return impl_->EnqueueFrame(data, size, user_private_data);
}

StatusCode Decoder::DequeueFrame(const DecoderBuffer** out_ptr) {
  if (!initialized_) return kLibgav1StatusNotInitialized;
  return impl_->DequeueFrame(out_ptr);
}

int Decoder::GetMaxAllowedFrames() const {
  if (!initialized_) return 1;
  return impl_->GetMaxAllowedFrames();
}

// static.
int Decoder::GetMaxBitdepth() { return DecoderImpl::GetMaxBitdepth(); }

}  // namespace libgav1
