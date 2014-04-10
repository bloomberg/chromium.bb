// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_JS_SYNC_JS_CONTROLLER_H_
#define SYNC_JS_SYNC_JS_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/js/js_controller.h"
#include "sync/js/js_event_handler.h"

namespace syncer {

class JsBackend;

// A class that mediates between the sync JsEventHandlers and the sync
// JsBackend.
class SYNC_EXPORT SyncJsController
    : public JsController, public JsEventHandler,
      public base::SupportsWeakPtr<SyncJsController> {
 public:
  SyncJsController();

  virtual ~SyncJsController();

  // Sets the backend to route all messages to (if initialized).
  // Sends any queued-up messages if |backend| is initialized.
  void AttachJsBackend(const WeakHandle<JsBackend>& js_backend);

  // JsController implementation.
  virtual void AddJsEventHandler(JsEventHandler* event_handler) OVERRIDE;
  virtual void RemoveJsEventHandler(JsEventHandler* event_handler) OVERRIDE;

  // JsEventHandler implementation.
  virtual void HandleJsEvent(const std::string& name,
                             const JsEventDetails& details) OVERRIDE;

 private:
  // Sets |js_backend_|'s event handler depending on how many
  // underlying event handlers we have.
  void UpdateBackendEventHandler();

  WeakHandle<JsBackend> js_backend_;
  ObserverList<JsEventHandler> js_event_handlers_;

  DISALLOW_COPY_AND_ASSIGN(SyncJsController);
};

}  // namespace syncer

#endif  // SYNC_JS_SYNC_JS_CONTROLLER_H_
