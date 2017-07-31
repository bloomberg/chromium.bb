/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef XSSAuditorDelegate_h
#define XSSAuditorDelegate_h

#include <memory>
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/TextPosition.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class Document;
class EncodedFormData;

class XSSInfo {
  USING_FAST_MALLOC(XSSInfo);
  WTF_MAKE_NONCOPYABLE(XSSInfo);

 public:
  static std::unique_ptr<XSSInfo> Create(const String& original_url,
                                         bool did_block_entire_page,
                                         bool did_send_xss_protection_header) {
    return WTF::WrapUnique(new XSSInfo(original_url, did_block_entire_page,
                                       did_send_xss_protection_header));
  }

  String BuildConsoleError() const;
  bool IsSafeToSendToAnotherThread() const;

  String original_url_;
  bool did_block_entire_page_;
  bool did_send_xss_protection_header_;
  TextPosition text_position_;

 private:
  XSSInfo(const String& original_url,
          bool did_block_entire_page,
          bool did_send_xss_protection_header)
      : original_url_(original_url.IsolatedCopy()),
        did_block_entire_page_(did_block_entire_page),
        did_send_xss_protection_header_(did_send_xss_protection_header) {}
};

class XSSAuditorDelegate final {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(XSSAuditorDelegate);

 public:
  explicit XSSAuditorDelegate(Document*);
  DECLARE_TRACE();

  void DidBlockScript(const XSSInfo&);
  void SetReportURL(const KURL& url) { report_url_ = url; }

 private:
  RefPtr<EncodedFormData> GenerateViolationReport(const XSSInfo&);

  Member<Document> document_;
  bool did_send_notifications_;
  KURL report_url_;
};

typedef Vector<std::unique_ptr<XSSInfo>> XSSInfoStream;

}  // namespace blink

#endif
