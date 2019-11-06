// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/mock_media_session_service_impl.h"

namespace content {

MockMediaSessionClient::MockMediaSessionClient() = default;

MockMediaSessionClient::~MockMediaSessionClient() = default;

blink::mojom::MediaSessionClientPtr
MockMediaSessionClient::CreateInterfacePtrAndBind() {
  blink::mojom::MediaSessionClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  return client;
}

MockMediaSessionServiceImpl::MockMediaSessionServiceImpl(
    content::RenderFrameHost* rfh)
    : MediaSessionServiceImpl(rfh) {
  SetClient(mock_client_.CreateInterfacePtrAndBind());
}

MockMediaSessionServiceImpl::~MockMediaSessionServiceImpl() = default;

}  // namespace content
