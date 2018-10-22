// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_DECODER_DECODER_ENGINE_H_
#define CHROMEOS_SERVICES_IME_DECODER_DECODER_ENGINE_H_

#include "base/scoped_native_library.h"
#include "chromeos/services/ime/input_engine.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace ime {

// TODO(https://crbug.com/837156): Introduce a DecoderAPILibrary class/struct.
// Inside, we define the shared function pointer types which are implemented
// in the decoder shared library.

// An enhanced implementation of the basic InputEngine which allows the input
// engine to call a customized transliteration library (aka decoder) to provide
// a premium typing experience.
class DecoderEngine : public InputEngine {
 public:
  DecoderEngine(service_manager::Connector* connector,
                scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~DecoderEngine() override;

  // InputEngine overrides:
  bool BindRequest(const std::string& ime_spec,
                   mojom::InputChannelRequest request,
                   mojom::InputChannelPtr client,
                   const std::vector<uint8_t>& extra) override;
  bool IsImeSupported(const std::string& ime_spec) override;
  void ProcessMessage(const std::vector<uint8_t>& message,
                      ProcessMessageCallback callback) override;

 private:
  // Shared library handle of the implementation for input logic with decoders.
  base::ScopedNativeLibrary library_;

  mojo::BindingSet<mojom::InputChannel> channel_bindings_;

  DISALLOW_COPY_AND_ASSIGN(DecoderEngine);
};

}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_DECODER_DECODER_ENGINE_H_
