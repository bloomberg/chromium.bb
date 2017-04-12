// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorProxy_h
#define CompositorProxy_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/CompositorProxyClient.h"
#include "core/dom/Element.h"
#include "core/geometry/DOMMatrix.h"
#include "platform/graphics/CompositorMutableState.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class DOMMatrix;
class ExceptionState;
class ExecutionContext;

// Owned by the main thread or control thread.
class CORE_EXPORT CompositorProxy final
    : public GarbageCollectedFinalized<CompositorProxy>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CompositorProxy* Create(ExecutionContext*,
                                 Element*,
                                 const Vector<String>& attribute_array,
                                 ExceptionState&);
  static CompositorProxy* Create(ExecutionContext*,
                                 uint64_t element,
                                 uint32_t compositor_mutable_properties);
  virtual ~CompositorProxy();

  DECLARE_TRACE();

  uint64_t ElementId() const { return element_id_; }
  uint32_t CompositorMutableProperties() const {
    return compositor_mutable_properties_;
  }
  bool supports(const String& attribute) const;

  bool initialized() const { return connected_ && state_.get(); }
  bool Connected() const { return connected_; }
  void disconnect();

  double opacity(ExceptionState&) const;
  double scrollLeft(ExceptionState&) const;
  double scrollTop(ExceptionState&) const;
  DOMMatrix* transform(ExceptionState&) const;

  void setOpacity(double, ExceptionState&);
  void setScrollLeft(double, ExceptionState&);
  void setScrollTop(double, ExceptionState&);
  void setTransform(DOMMatrix*, ExceptionState&);

  void TakeCompositorMutableState(std::unique_ptr<CompositorMutableState>);

 protected:
  CompositorProxy(uint64_t element_id, uint32_t compositor_mutable_properties);
  CompositorProxy(Element&, const Vector<String>& attribute_array);
  CompositorProxy(uint64_t element,
                  uint32_t compositor_mutable_properties,
                  CompositorProxyClient*);

 private:
  bool RaiseExceptionIfNotMutable(uint32_t compositor_mutable_property,
                                  ExceptionState&) const;
  void DisconnectInternal();

  const uint64_t element_id_ = 0;
  const uint32_t compositor_mutable_properties_ = 0;

  bool connected_ = true;
  Member<CompositorProxyClient> client_;
  std::unique_ptr<CompositorMutableState> state_;
};

}  // namespace blink

#endif  // CompositorProxy_h
