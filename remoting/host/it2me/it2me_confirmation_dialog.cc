// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_confirmation_dialog.h"

namespace remoting {

It2MeConfirmationDialogFactory::It2MeConfirmationDialogFactory() {}

It2MeConfirmationDialogFactory::~It2MeConfirmationDialogFactory() {}

// TODO(dcaiafa): Remove after implementations for all platforms exist.
#if !defined(OS_CHROMEOS)
scoped_ptr<It2MeConfirmationDialog> It2MeConfirmationDialogFactory::Create() {
  return nullptr;
}
#endif

}  // namespace remoting
