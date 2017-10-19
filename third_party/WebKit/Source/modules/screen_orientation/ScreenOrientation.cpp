// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/screen_orientation/ScreenOrientation.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "modules/EventTargetModules.h"
#include "modules/screen_orientation/LockOrientationCallback.h"
#include "modules/screen_orientation/ScreenOrientationControllerImpl.h"
#include "platform/wtf/Assertions.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationType.h"

// This code assumes that WebScreenOrientationType values are included in
// WebScreenOrientationLockType.
STATIC_ASSERT_ENUM(blink::kWebScreenOrientationPortraitPrimary,
                   blink::kWebScreenOrientationLockPortraitPrimary);
STATIC_ASSERT_ENUM(blink::kWebScreenOrientationPortraitSecondary,
                   blink::kWebScreenOrientationLockPortraitSecondary);
STATIC_ASSERT_ENUM(blink::kWebScreenOrientationLandscapePrimary,
                   blink::kWebScreenOrientationLockLandscapePrimary);
STATIC_ASSERT_ENUM(blink::kWebScreenOrientationLandscapeSecondary,
                   blink::kWebScreenOrientationLockLandscapeSecondary);

namespace blink {

struct ScreenOrientationInfo {
  const AtomicString& name;
  unsigned orientation;
};

static ScreenOrientationInfo* OrientationsMap(unsigned& length) {
  DEFINE_STATIC_LOCAL(const AtomicString, portrait_primary,
                      ("portrait-primary"));
  DEFINE_STATIC_LOCAL(const AtomicString, portrait_secondary,
                      ("portrait-secondary"));
  DEFINE_STATIC_LOCAL(const AtomicString, landscape_primary,
                      ("landscape-primary"));
  DEFINE_STATIC_LOCAL(const AtomicString, landscape_secondary,
                      ("landscape-secondary"));
  DEFINE_STATIC_LOCAL(const AtomicString, any, ("any"));
  DEFINE_STATIC_LOCAL(const AtomicString, portrait, ("portrait"));
  DEFINE_STATIC_LOCAL(const AtomicString, landscape, ("landscape"));
  DEFINE_STATIC_LOCAL(const AtomicString, natural, ("natural"));

  static ScreenOrientationInfo orientation_map[] = {
      {portrait_primary, kWebScreenOrientationLockPortraitPrimary},
      {portrait_secondary, kWebScreenOrientationLockPortraitSecondary},
      {landscape_primary, kWebScreenOrientationLockLandscapePrimary},
      {landscape_secondary, kWebScreenOrientationLockLandscapeSecondary},
      {any, kWebScreenOrientationLockAny},
      {portrait, kWebScreenOrientationLockPortrait},
      {landscape, kWebScreenOrientationLockLandscape},
      {natural, kWebScreenOrientationLockNatural}};
  length = WTF_ARRAY_LENGTH(orientation_map);

  return orientation_map;
}

const AtomicString& ScreenOrientation::OrientationTypeToString(
    WebScreenOrientationType orientation) {
  unsigned length = 0;
  ScreenOrientationInfo* orientation_map = OrientationsMap(length);
  for (unsigned i = 0; i < length; ++i) {
    if (static_cast<unsigned>(orientation) == orientation_map[i].orientation)
      return orientation_map[i].name;
  }

  NOTREACHED();
  return g_null_atom;
}

static WebScreenOrientationLockType StringToOrientationLock(
    const AtomicString& orientation_lock_string) {
  unsigned length = 0;
  ScreenOrientationInfo* orientation_map = OrientationsMap(length);
  for (unsigned i = 0; i < length; ++i) {
    if (orientation_map[i].name == orientation_lock_string)
      return static_cast<WebScreenOrientationLockType>(
          orientation_map[i].orientation);
  }

  NOTREACHED();
  return kWebScreenOrientationLockDefault;
}

// static
ScreenOrientation* ScreenOrientation::Create(LocalFrame* frame) {
  DCHECK(frame);

  // Check if the ScreenOrientationController is supported for the
  // frame. It will not be for all LocalFrames, or the frame may
  // have been detached.
  if (!ScreenOrientationControllerImpl::From(*frame))
    return nullptr;

  ScreenOrientation* orientation = new ScreenOrientation(frame);
  DCHECK(orientation->Controller());
  // FIXME: ideally, we would like to provide the ScreenOrientationController
  // the case where it is not defined but for the moment, it is eagerly
  // created when the LocalFrame is created so we shouldn't be in that
  // situation.
  // In order to create the ScreenOrientationController lazily, we would need
  // to be able to access WebFrameClient from modules/.

  orientation->Controller()->SetOrientation(orientation);
  return orientation;
}

ScreenOrientation::ScreenOrientation(LocalFrame* frame)
    : ContextClient(frame), type_(kWebScreenOrientationUndefined), angle_(0) {}

ScreenOrientation::~ScreenOrientation() {}

const WTF::AtomicString& ScreenOrientation::InterfaceName() const {
  return EventTargetNames::ScreenOrientation;
}

ExecutionContext* ScreenOrientation::GetExecutionContext() const {
  if (!GetFrame())
    return nullptr;
  return GetFrame()->GetDocument();
}

String ScreenOrientation::type() const {
  return OrientationTypeToString(type_);
}

unsigned short ScreenOrientation::angle() const {
  return angle_;
}

void ScreenOrientation::SetType(WebScreenOrientationType type) {
  type_ = type;
}

void ScreenOrientation::SetAngle(unsigned short angle) {
  angle_ = angle;
}

ScriptPromise ScreenOrientation::lock(ScriptState* state,
                                      const AtomicString& lock_string) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(state);
  ScriptPromise promise = resolver->Promise();

  Document* document = GetFrame() ? GetFrame()->GetDocument() : nullptr;

  if (!document || !Controller()) {
    DOMException* exception = DOMException::Create(
        kInvalidStateError,
        "The object is no longer associated to a document.");
    resolver->Reject(exception);
    return promise;
  }

  if (document->IsSandboxed(kSandboxOrientationLock)) {
    DOMException* exception =
        DOMException::Create(kSecurityError,
                             "The document is sandboxed and lacks the "
                             "'allow-orientation-lock' flag.");
    resolver->Reject(exception);
    return promise;
  }

  Controller()->lock(StringToOrientationLock(lock_string),
                     WTF::MakeUnique<LockOrientationCallback>(resolver));
  return promise;
}

void ScreenOrientation::unlock() {
  if (!Controller())
    return;

  Controller()->unlock();
}

ScreenOrientationControllerImpl* ScreenOrientation::Controller() {
  if (!GetFrame())
    return nullptr;

  return ScreenOrientationControllerImpl::From(*GetFrame());
}

void ScreenOrientation::Trace(blink::Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
