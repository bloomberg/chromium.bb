// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/mus_views_delegate.h"

#include "ui/views/mus/ax_remote_host.h"
#include "ui/views/mus/mus_client.h"

namespace views {

MusViewsDelegate::MusViewsDelegate() = default;

MusViewsDelegate::~MusViewsDelegate() = default;

void MusViewsDelegate::NotifyAccessibilityEvent(View* view,
                                                ax::mojom::Event event_type) {
  if (MusClient::Get()->ax_remote_host())
    MusClient::Get()->ax_remote_host()->HandleEvent(view, event_type);
}

}  // namespace views
