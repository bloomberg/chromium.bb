// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BADGING_NAVIGATOR_BADGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BADGING_NAVIGATOR_BADGE_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/badging/badging.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
class Navigator;
class ScriptPromise;
class WorkerNavigator;

class NavigatorBadge final : public GarbageCollected<NavigatorBadge>,
                             public Supplement<ExecutionContext> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorBadge);

 public:
  static const char kSupplementName[];

  static NavigatorBadge& From(ScriptState*);
  explicit NavigatorBadge(ExecutionContext*);

  // Badge IDL interface.
  static ScriptPromise setAppBadge(ScriptState*, Navigator&);
  static ScriptPromise setAppBadge(ScriptState*, WorkerNavigator&);

  static ScriptPromise setAppBadge(ScriptState*, Navigator&, uint64_t content);
  static ScriptPromise setAppBadge(ScriptState*,
                                   WorkerNavigator&,
                                   uint64_t content);

  static ScriptPromise clearAppBadge(ScriptState*, Navigator&);
  static ScriptPromise clearAppBadge(ScriptState*, WorkerNavigator&);

  void Trace(Visitor*) override;

 private:
  static ScriptPromise SetAppBadgeHelper(
      ScriptState* script_state,
      mojom::blink::BadgeValuePtr badge_value);
  static ScriptPromise ClearAppBadgeHelper(ScriptState* script_state);

  mojo::Remote<mojom::blink::BadgeService> badge_service();

  Member<ExecutionContext> context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BADGING_NAVIGATOR_BADGE_H_
