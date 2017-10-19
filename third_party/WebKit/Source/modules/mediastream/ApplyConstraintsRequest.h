// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ApplyConstraintsRequest_h
#define ApplyConstraintsRequest_h

#include "modules/ModulesExport.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Visitor.h"
#include "public/platform/WebMediaConstraints.h"
#include "public/platform/WebMediaStreamTrack.h"

namespace blink {

class ScriptPromiseResolver;

class MODULES_EXPORT ApplyConstraintsRequest final
    : public GarbageCollectedFinalized<ApplyConstraintsRequest> {
 public:
  static ApplyConstraintsRequest* Create(const WebMediaStreamTrack&,
                                         const WebMediaConstraints&,
                                         ScriptPromiseResolver*);
  static ApplyConstraintsRequest* CreateForTesting(const WebMediaStreamTrack&,
                                                   const WebMediaConstraints&);

  WebMediaStreamTrack Track() const;
  WebMediaConstraints Constraints() const;

  void RequestSucceeded();
  void RequestFailed(const String& constraint, const String& message);

  virtual void Trace(blink::Visitor*);

 private:
  ApplyConstraintsRequest(const WebMediaStreamTrack&,
                          const WebMediaConstraints&,
                          ScriptPromiseResolver*);

  WebMediaStreamTrack track_;
  WebMediaConstraints constraints_;
  Member<ScriptPromiseResolver> resolver_;
};

}  // namespace blink

#endif  // ApplyConstraintsRequest_h
