// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentShutdownNotifier_h
#define DocumentShutdownNotifier_h

#include "core/CoreExport.h"
#include "platform/LifecycleNotifier.h"

namespace blink {

class Document;
class DocumentShutdownObserver;

// Sibling class of DocumentShutdownObserver; implemented by Document to notify
// subclasses of DocumentShutdownObserver of Document shutdown.
class CORE_EXPORT DocumentShutdownNotifier
    : public LifecycleNotifier<Document, DocumentShutdownObserver> {
 protected:
  DocumentShutdownNotifier();

 private:
  DISALLOW_COPY_AND_ASSIGN(DocumentShutdownNotifier);
};

}  // namespace blink

#endif  // DocumentShutdownNotifier_h
