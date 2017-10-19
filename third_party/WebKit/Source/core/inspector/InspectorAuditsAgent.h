// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorAuditsAgent_h
#define InspectorAuditsAgent_h

#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Audits.h"

namespace blink {

class CORE_EXPORT InspectorAuditsAgent final
    : public InspectorBaseAgent<protocol::Audits::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorAuditsAgent);

 public:
  explicit InspectorAuditsAgent(InspectorNetworkAgent*);
  ~InspectorAuditsAgent() override;

  virtual void Trace(blink::Visitor*);

  // Protocol methods.
  protocol::Response getEncodedResponse(const String& request_id,
                                        const String& encoding,
                                        protocol::Maybe<double> quality,
                                        protocol::Maybe<bool> size_only,
                                        protocol::Maybe<String>* out_body,
                                        int* out_original_size,
                                        int* out_encoded_size) override;

 private:
  Member<InspectorNetworkAgent> network_agent_;
};

}  // namespace blink

#endif  // !defined(InspectorAuditsAgent_h)
