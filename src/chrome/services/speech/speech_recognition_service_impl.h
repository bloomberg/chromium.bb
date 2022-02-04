// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_SPEECH_RECOGNITION_SERVICE_IMPL_H_
#define CHROME_SERVICES_SPEECH_SPEECH_RECOGNITION_SERVICE_IMPL_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace speech {

// Implements the SpeechRecognitionService with SODA on-device speech
// recognition.
class SpeechRecognitionServiceImpl
    : public media::mojom::SpeechRecognitionService,
      public media::mojom::SpeechRecognitionContext {
 public:
  explicit SpeechRecognitionServiceImpl(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionService> receiver);

  SpeechRecognitionServiceImpl(const SpeechRecognitionServiceImpl&) = delete;
  SpeechRecognitionServiceImpl& operator=(const SpeechRecognitionServiceImpl&) =
      delete;

  ~SpeechRecognitionServiceImpl() override;

  // media::mojom::SpeechRecognitionService
  void BindContext(mojo::PendingReceiver<media::mojom::SpeechRecognitionContext>
                       context) override;
  void SetSodaPath(const base::FilePath& binary_path,
                   const base::FilePath& config_path) override;

  // media::mojom::SpeechRecognitionContext
  void BindRecognizer(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options,
      BindRecognizerCallback callback) override;
  void BindAudioSourceFetcher(
      mojo::PendingReceiver<media::mojom::AudioSourceFetcher> fetcher_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options,
      BindRecognizerCallback callback) override;

 protected:
  void DisconnectHandler();

  mojo::Receiver<media::mojom::SpeechRecognitionService> receiver_;

  // The set of receivers used to receive messages from the renderer clients.
  mojo::ReceiverSet<media::mojom::SpeechRecognitionContext>
      speech_recognition_contexts_;

  base::FilePath binary_path_ = base::FilePath();
  base::FilePath config_path_ = base::FilePath();

  base::WeakPtrFactory<SpeechRecognitionServiceImpl> weak_factory_{this};
};

}  // namespace speech

#endif  // CHROME_SERVICES_SPEECH_SPEECH_RECOGNITION_SERVICE_IMPL_H_
