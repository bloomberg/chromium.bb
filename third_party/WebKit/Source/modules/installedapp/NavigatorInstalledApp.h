// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorInstalledApp_h
#define NavigatorInstalledApp_h

#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class Document;
class Navigator;
class ScriptPromise;
class ScriptState;
class InstalledAppController;

class NavigatorInstalledApp final
    : public GarbageCollected<NavigatorInstalledApp>,
      public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorInstalledApp);

 public:
  static NavigatorInstalledApp* From(Document&);
  static NavigatorInstalledApp& From(Navigator&);

  static ScriptPromise getInstalledRelatedApps(ScriptState*, Navigator&);
  ScriptPromise getInstalledRelatedApps(ScriptState*);

  InstalledAppController* Controller();

  virtual void Trace(blink::Visitor*);

 private:
  explicit NavigatorInstalledApp(Navigator&);
  static const char* SupplementName();
};

}  // namespace blink

#endif  // NavigatorInstalledApp_h
