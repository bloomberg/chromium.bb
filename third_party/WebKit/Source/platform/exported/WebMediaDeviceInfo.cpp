/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/platform/WebMediaDeviceInfo.h"

#include "base/memory/scoped_refptr.h"
#include "platform/wtf/RefCounted.h"
#include "public/platform/WebString.h"

namespace blink {

class WebMediaDeviceInfoPrivate final
    : public RefCounted<WebMediaDeviceInfoPrivate> {
 public:
  static scoped_refptr<WebMediaDeviceInfoPrivate> Create(
      const WebString& device_id,
      WebMediaDeviceInfo::MediaDeviceKind,
      const WebString& label,
      const WebString& group_id);

  const WebString& DeviceId() const { return device_id_; }
  WebMediaDeviceInfo::MediaDeviceKind Kind() const { return kind_; }
  const WebString& Label() const { return label_; }
  const WebString& GroupId() const { return group_id_; }

 private:
  WebMediaDeviceInfoPrivate(const WebString& device_id,
                            WebMediaDeviceInfo::MediaDeviceKind,
                            const WebString& label,
                            const WebString& group_id);

  WebString device_id_;
  WebMediaDeviceInfo::MediaDeviceKind kind_;
  WebString label_;
  WebString group_id_;
};

scoped_refptr<WebMediaDeviceInfoPrivate> WebMediaDeviceInfoPrivate::Create(
    const WebString& device_id,
    WebMediaDeviceInfo::MediaDeviceKind kind,
    const WebString& label,
    const WebString& group_id) {
  return base::AdoptRef(
      new WebMediaDeviceInfoPrivate(device_id, kind, label, group_id));
}

WebMediaDeviceInfoPrivate::WebMediaDeviceInfoPrivate(
    const WebString& device_id,
    WebMediaDeviceInfo::MediaDeviceKind kind,
    const WebString& label,
    const WebString& group_id)
    : device_id_(device_id), kind_(kind), label_(label), group_id_(group_id) {}

void WebMediaDeviceInfo::Assign(const WebMediaDeviceInfo& other) {
  private_ = other.private_;
}

void WebMediaDeviceInfo::Reset() {
  private_.Reset();
}

void WebMediaDeviceInfo::Initialize(const WebString& device_id,
                                    WebMediaDeviceInfo::MediaDeviceKind kind,
                                    const WebString& label,
                                    const WebString& group_id) {
  private_ =
      WebMediaDeviceInfoPrivate::Create(device_id, kind, label, group_id);
}

WebString WebMediaDeviceInfo::DeviceId() const {
  DCHECK(!private_.IsNull());
  return private_->DeviceId();
}

WebMediaDeviceInfo::MediaDeviceKind WebMediaDeviceInfo::Kind() const {
  DCHECK(!private_.IsNull());
  return private_->Kind();
}

WebString WebMediaDeviceInfo::Label() const {
  DCHECK(!private_.IsNull());
  return private_->Label();
}

WebString WebMediaDeviceInfo::GroupId() const {
  DCHECK(!private_.IsNull());
  return private_->GroupId();
}

}  // namespace blink
