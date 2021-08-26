// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/decoder/decoder_engine.h"

#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chromeos/services/ime/constants.h"

namespace chromeos {
namespace ime {

namespace {

// A client delegate passed to the shared library in order for the
// shared library to send replies back to the engine.
class ClientDelegate : public ImeClientDelegate {
 public:
  // All replies from the shared library will be sent to both |remote| and
  // |callback|.
  ClientDelegate(const std::string& ime_spec,
                 mojo::PendingRemote<mojom::InputChannel> remote)
      : ime_spec_(ime_spec), client_remote_(std::move(remote)) {
    client_remote_.set_disconnect_handler(base::BindOnce(
        &ClientDelegate::OnDisconnected, base::Unretained(this)));
  }

  ~ClientDelegate() override {}

  const char* ImeSpec() override { return ime_spec_.c_str(); }

  void Process(const uint8_t* data, size_t size) override {
    if (client_remote_ && client_remote_.is_bound()) {
      std::vector<uint8_t> msg(data, data + size);
      client_remote_->ProcessMessage(msg, base::DoNothing());
    }
  }

  void Destroy() override {}

 private:
  void OnDisconnected() {
    client_remote_.reset();
    LOG(ERROR) << "Client remote is disconnected." << ime_spec_;
  }

  // The ime specification which is unique in the scope of engine.
  std::string ime_spec_;

  // The InputChannel remote used to talk to the client.
  mojo::Remote<mojom::InputChannel> client_remote_;
};

}  // namespace

DecoderEngine::DecoderEngine(ImeCrosPlatform* platform) : platform_(platform) {
  if (!TryLoadDecoder()) {
    LOG(WARNING) << "DecoderEngine INIT INCOMPLETED.";
  }
}

DecoderEngine::~DecoderEngine() {}

bool DecoderEngine::TryLoadDecoder() {
  auto* decoder = ImeDecoder::GetInstance();
  if (decoder->GetStatus() == ImeDecoder::Status::kSuccess &&
      decoder->GetEntryPoints().is_ready) {
    decoder_entry_points_ = decoder->GetEntryPoints();
    decoder_entry_points_->init_once(platform_);
    return true;
  }
  return false;
}

bool DecoderEngine::BindRequest(
    const std::string& ime_spec,
    mojo::PendingReceiver<mojom::InputChannel> receiver,
    mojo::PendingRemote<mojom::InputChannel> remote,
    const std::vector<uint8_t>& extra) {
  if (IsImeSupportedByDecoder(ime_spec)) {
    // Activates an IME engine via the shared library. Passing a
    // |ClientDelegate| for engine instance created by the shared library to
    // make safe calls on the client.
    if (decoder_entry_points_->activate_ime(
            ime_spec.c_str(),
            new ClientDelegate(ime_spec, std::move(remote)))) {
      decoder_channel_receivers_.Add(this, std::move(receiver));
      // TODO(https://crbug.com/837156): Registry connection error handler.
      return true;
    }
    return false;
  }
  return false;
}

bool DecoderEngine::IsImeSupportedByDecoder(const std::string& ime_spec) {
  return decoder_entry_points_ &&
         decoder_entry_points_->supports(ime_spec.c_str());
}

void DecoderEngine::ProcessMessage(const std::vector<uint8_t>& message,
                                   ProcessMessageCallback callback) {
  // TODO(https://crbug.com/837156): Set a default protobuf message.
  std::vector<uint8_t> result;

  // Handle message via corresponding functions of loaded decoder.
  if (decoder_entry_points_) {
    decoder_entry_points_->process(message.data(), message.size());
  }

  std::move(callback).Run(result);
}

}  // namespace ime
}  // namespace chromeos
