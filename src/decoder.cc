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

#include <new>

#include "src/decoder_impl.h"

extern "C" {

Libgav1Decoder* Libgav1DecoderCreate() {
  return reinterpret_cast<Libgav1Decoder*>(new (std::nothrow)
                                               libgav1::Decoder());
}

void Libgav1DecoderDestroy(Libgav1Decoder* decoder) {
  auto* cxx_decoder = reinterpret_cast<libgav1::Decoder*>(decoder);
  delete cxx_decoder;
}

Libgav1StatusCode Libgav1DecoderInit(Libgav1Decoder* decoder,
                                     const Libgav1DecoderSettings* settings) {
  auto* cxx_decoder = reinterpret_cast<libgav1::Decoder*>(decoder);
  libgav1::DecoderSettings cxx_settings;
  cxx_settings.threads = settings->threads;
  cxx_settings.frame_parallel = settings->frame_parallel != 0;
  cxx_settings.get = settings->get;
  cxx_settings.release = settings->release;
  cxx_settings.on_frame_buffer_size_changed =
      settings->on_frame_buffer_size_changed;
  cxx_settings.get_frame_buffer = settings->get_frame_buffer;
  cxx_settings.release_frame_buffer = settings->release_frame_buffer;
  cxx_settings.callback_private_data = settings->callback_private_data;
  cxx_settings.post_filter_mask = settings->post_filter_mask;
  return cxx_decoder->Init(&cxx_settings);
}

Libgav1StatusCode Libgav1DecoderEnqueueFrame(Libgav1Decoder* decoder,
                                             const uint8_t* data, size_t size,
                                             int64_t user_private_data) {
  auto* cxx_decoder = reinterpret_cast<libgav1::Decoder*>(decoder);
  return cxx_decoder->EnqueueFrame(data, size, user_private_data);
}

Libgav1StatusCode Libgav1DecoderDequeueFrame(
    Libgav1Decoder* decoder, const Libgav1DecoderBuffer** out_ptr) {
  auto* cxx_decoder = reinterpret_cast<libgav1::Decoder*>(decoder);
  return cxx_decoder->DequeueFrame(out_ptr);
}

Libgav1StatusCode Libgav1DecoderSignalEOS(Libgav1Decoder* decoder) {
  auto* cxx_decoder = reinterpret_cast<libgav1::Decoder*>(decoder);
  return cxx_decoder->SignalEOS();
}

int Libgav1DecoderGetMaxAllowedFrames(const Libgav1Decoder* decoder) {
  const auto* cxx_decoder = reinterpret_cast<const libgav1::Decoder*>(decoder);
  return cxx_decoder->GetMaxAllowedFrames();
}

int Libgav1DecoderGetMaxBitdepth() {
  return libgav1::Decoder::GetMaxBitdepth();
}

}  // extern "C"

namespace libgav1 {

Decoder::Decoder() = default;

Decoder::~Decoder() = default;

StatusCode Decoder::Init(const DecoderSettings* const settings) {
  if (impl_ != nullptr) return kLibgav1StatusAlready;
  if (settings != nullptr) settings_ = *settings;
  return DecoderImpl::Create(&settings_, &impl_);
}

StatusCode Decoder::EnqueueFrame(const uint8_t* data, const size_t size,
                                 int64_t user_private_data) {
  if (impl_ == nullptr) return kLibgav1StatusNotInitialized;
  return impl_->EnqueueFrame(data, size, user_private_data);
}

StatusCode Decoder::DequeueFrame(const DecoderBuffer** out_ptr) {
  if (impl_ == nullptr) return kLibgav1StatusNotInitialized;
  return impl_->DequeueFrame(out_ptr);
}

StatusCode Decoder::SignalEOS() {
  if (impl_ == nullptr) return kLibgav1StatusNotInitialized;
  // In non-frame-parallel mode, we have to release all the references. This
  // simply means replacing the |impl_| with a new instance so that all the
  // existing references are released and the state is cleared.
  impl_ = nullptr;
  return DecoderImpl::Create(&settings_, &impl_);
}

int Decoder::GetMaxAllowedFrames() const {
  if (impl_ == nullptr) return 1;
  return impl_->GetMaxAllowedFrames();
}

// static.
int Decoder::GetMaxBitdepth() { return DecoderImpl::GetMaxBitdepth(); }

}  // namespace libgav1
