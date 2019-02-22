// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/audio_focus_observer.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

AudioFocusObserver::AudioFocusObserver() : binding_(this) {}

void AudioFocusObserver::RegisterAudioFocusObserver() {
  ConnectToService();

  if (!audio_focus_ptr_.is_bound() || audio_focus_ptr_.encountered_error())
    return;

  if (binding_.is_bound())
    return;

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  media_session::mojom::AudioFocusObserverPtr observer;
  binding_.Bind(mojo::MakeRequest(&observer));
  audio_focus_ptr_->AddObserver(std::move(observer));
}

void AudioFocusObserver::UnregisterAudioFocusObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  binding_.Close();
}

void AudioFocusObserver::ConnectToService() {
  if (audio_focus_ptr_.encountered_error())
    audio_focus_ptr_.reset();

  if (audio_focus_ptr_.is_bound())
    return;

  ServiceManagerConnection* connection =
      ServiceManagerConnection::GetForProcess();

  if (!connection)
    return;

  service_manager::Connector* connector = connection->GetConnector();
  connector->BindInterface(media_session::mojom::kServiceName,
                           mojo::MakeRequest(&audio_focus_ptr_));
}

AudioFocusObserver::~AudioFocusObserver() = default;

}  // namespace content
