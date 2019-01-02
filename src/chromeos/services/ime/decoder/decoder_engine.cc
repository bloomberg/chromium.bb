// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/decoder/decoder_engine.h"

#include "base/files/file_path.h"
#include "build/buildflag.h"
#include "chromeos/services/ime/public/cpp/buildflags.h"

namespace chromeos {
namespace ime {

namespace {

#if BUILDFLAG(ENABLE_CROS_IME_EXAMPLE_SO)
const char kDecoderLibName[] = "input_decoder_example";
#else
const char kDecoderLibName[] = "input_decoder_engine";
#endif

}  // namespace

DecoderEngine::DecoderEngine(
    service_manager::Connector* connector,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  // TODO(https://crbug.com/837156): Connect utility services for InputEngine
  // via connector(). Eg. take advantage of the file and network services to
  // download relevant language models required by the decocers to the device.

  // Load the decoder library.
  base::NativeLibraryLoadError load_error;
  base::FilePath lib_path(base::GetNativeLibraryName(kDecoderLibName));
  library_.Reset(base::LoadNativeLibrary(lib_path, &load_error));

  if (!library_.is_valid()) {
    LOG(ERROR) << "Failed to load a decoder shared library.";
    return;
  }

  // TODO(https://crbug.com/837156): Based on the defined function pointer types
  // in DecoderAPILibrary, load all required function pointers.
}

DecoderEngine::~DecoderEngine() {}

bool DecoderEngine::BindRequest(const std::string& ime_spec,
                                mojom::InputChannelRequest request,
                                mojom::InputChannelPtr client,
                                const std::vector<uint8_t>& extra) {
  if (!IsImeSupported(ime_spec))
    return false;
  // TODO(https://crbug.com/837156): Notify the input logic loaded from the
  // shared library about the change by forwarding the information received.
  // Moreover, in order to make safe calls on the clientPtr inside the input
  // logic with multiple threads, we will send a wrapper of the clientPtr,
  // where we make sure it runs in the correct SequencedTaskRunner.
  channel_bindings_.AddBinding(this, std::move(request));
  // TODO(https://crbug.com/837156): Registry connection error handler.
  return true;
}

bool DecoderEngine::IsImeSupported(const std::string& ime_spec) {
  // TODO(https://crbug.com/837156): Check the capability of the loaded shared
  // library first.
  return InputEngine::IsImeSupported(ime_spec);
}

void DecoderEngine::ProcessMessage(const std::vector<uint8_t>& message,
                                   ProcessMessageCallback callback) {
  std::vector<uint8_t> result;

  // TODO(https://crbug.com/837156): Implement the functions below by calling
  // the corresponding functions of the shared library loaded.
  std::move(callback).Run(result);
}

}  // namespace ime
}  // namespace chromeos
