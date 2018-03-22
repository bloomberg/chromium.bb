// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_OUTPUT_STREAM_H_
#define SERVICES_AUDIO_OUTPUT_STREAM_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/sync_socket.h"
#include "base/timer/timer.h"
#include "media/audio/audio_output_controller.h"
#include "media/mojo/interfaces/audio_data_pipe.mojom.h"
#include "media/mojo/interfaces/audio_logging.mojom.h"
#include "media/mojo/interfaces/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace media {
class AudioManager;
class AudioParameters;
class AudioSyncReader;
}  // namespace media

namespace audio {

class OutputStream final : public media::mojom::AudioOutputStream,
                           public media::AudioOutputController::EventHandler {
 public:
  using DeleteCallback = base::OnceCallback<void(OutputStream*)>;
  using CreatedCallback =
      base::OnceCallback<void(media::mojom::AudioDataPipePtr)>;

  OutputStream(CreatedCallback created_callback,
               DeleteCallback delete_callback,
               media::mojom::AudioOutputStreamRequest stream_request,
               media::mojom::AudioOutputStreamClientPtr client,
               media::mojom::AudioOutputStreamObserverAssociatedPtr observer,
               media::mojom::AudioLogPtr log,
               media::AudioManager* audio_manager,
               const std::string& output_device_id,
               const media::AudioParameters& params,
               const base::UnguessableToken& group_id);

  ~OutputStream() final;

  // media::mojom::AudioOutputStream implementation.
  void Play() final;
  void Pause() final;
  void SetVolume(double volume) final;

  // AudioOutputController::EventHandler implementation.
  void OnControllerCreated() final;
  void OnControllerPlaying() final;
  void OnControllerPaused() final;
  void OnControllerError() final;
  void OnLog(base::StringPiece message) final;

 private:
  void OnError();
  void CallDeleter();
  void PollAudioLevel();
  bool IsAudible() const;

  SEQUENCE_CHECKER(owning_sequence_);

  base::CancelableSyncSocket foreign_socket_;
  CreatedCallback created_callback_;
  DeleteCallback delete_callback_;
  mojo::Binding<AudioOutputStream> binding_;
  media::mojom::AudioOutputStreamClientPtr client_;
  media::mojom::AudioOutputStreamObserverAssociatedPtr observer_;
  scoped_refptr<media::mojom::ThreadSafeAudioLogPtr> log_;

  const std::unique_ptr<media::AudioSyncReader> reader_;
  scoped_refptr<media::AudioOutputController> controller_;

  // This flag ensures that we only send OnStreamStateChanged notifications
  // and (de)register with the stream monitor when the state actually changes.
  bool playing_ = false;

  // Calls PollAudioLevel() at regular intervals while |playing_| is true.
  base::RepeatingTimer poll_timer_;
  bool is_audible_ = false;

  base::WeakPtrFactory<OutputStream> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(OutputStream);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_OUTPUT_STREAM_H_
