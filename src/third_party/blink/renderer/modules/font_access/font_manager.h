// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_MANAGER_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace blink {

class ScriptState;
class ScriptValue;
class ScriptPromise;
class QueryOptions;

class FontManager final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  FontManager() = default;

  // Disallow copy and assign.
  FontManager(const FontManager&) = delete;
  FontManager operator=(const FontManager&) = delete;

  // FontManager IDL interface implementation.
  ScriptValue query(ScriptState*, ExceptionState&);
  ScriptPromise showFontChooser(ScriptState*, const QueryOptions* options);

  void Trace(blink::Visitor*) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_MANAGER_H_
