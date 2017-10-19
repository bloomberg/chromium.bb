// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Report_h
#define Report_h

#include "core/frame/ReportBody.h"

namespace blink {

class CORE_EXPORT Report : public GarbageCollectedFinalized<Report>,
                           public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Report(const String& type, const String& url, ReportBody* body)
      : type_(type), url_(url), body_(body) {}

  virtual ~Report() {}

  String type() const { return type_; }
  String url() const { return url_; }
  ReportBody* body() const { return body_; }

  void Trace(blink::Visitor* visitor) { visitor->Trace(body_); }

 private:
  const String type_;
  const String url_;
  Member<ReportBody> body_;
};

}  // namespace blink

#endif  // Report_h
