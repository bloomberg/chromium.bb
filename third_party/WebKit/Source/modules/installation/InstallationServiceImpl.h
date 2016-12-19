// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InstallationServiceImpl_h
#define InstallationServiceImpl_h

#include "modules/ModulesExport.h"
#include "platform/heap/Persistent.h"
#include "public/platform/modules/installation/installation.mojom-blink.h"

namespace blink {

class LocalFrame;

class MODULES_EXPORT InstallationServiceImpl final
    : public mojom::blink::InstallationService {
 public:
  explicit InstallationServiceImpl(LocalFrame&);

  static void create(LocalFrame*, mojom::blink::InstallationServiceRequest);

  void OnInstall() override;

 private:
  WeakPersistent<LocalFrame> m_frame;
};

}  // namespace blink

#endif  // InstallationServiceImpl_h
