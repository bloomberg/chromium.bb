// Copyright 2017 The Chromium Authors. All rights reserved.  Use of
// this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentShutdownObserver_h
#define DocumentShutdownObserver_h

#include "core/CoreExport.h"
#include "core/dom/Document.h"
#include "platform/LifecycleObserver.h"

namespace blink {

// This class is a base class for classes which observe Document shutdown
// synchronously.
// Note: this functionality is also provided by SynchronousMutationObserver,
// but if you don't need to respond to the other events handled by that class,
// using this class is more efficient.
class CORE_EXPORT DocumentShutdownObserver
    : public LifecycleObserver<Document, DocumentShutdownObserver> {
 public:
  // Called when detaching document.
  virtual void ContextDestroyed(Document*);

 protected:
  DocumentShutdownObserver();

 private:
  DISALLOW_COPY_AND_ASSIGN(DocumentShutdownObserver);
};

}  // namespace blink

#endif  // DocumentShutdownObserver_h
