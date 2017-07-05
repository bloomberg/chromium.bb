// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementDefinitionBuilder_h
#define CustomElementDefinitionBuilder_h

#include "core/CoreExport.h"
#include "core/html/custom/CustomElementDefinition.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class CustomElementDescriptor;
class CustomElementRegistry;

// Implement CustomElementDefinitionBuilder to provide
// technology-specific steps for CustomElementRegistry.define.
// https://html.spec.whatwg.org/multipage/scripting.html#dom-customelementsregistry-define
class CORE_EXPORT CustomElementDefinitionBuilder {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(CustomElementDefinitionBuilder);

 public:
  CustomElementDefinitionBuilder() {}

  // This API necessarily sounds JavaScript specific; this implements
  // some steps of the CustomElementRegistry.define process, which
  // are defined in terms of JavaScript.

  // Check the constructor is valid. Return false if processing
  // should not proceed.
  virtual bool CheckConstructorIntrinsics() = 0;

  // Check the constructor is not already registered in the calling
  // registry. Return false if processing should not proceed.
  virtual bool CheckConstructorNotRegistered() = 0;

  // Checking the prototype may destroy the window. Return false if
  // processing should not proceed.
  virtual bool CheckPrototype() = 0;

  // Cache properties for build to use. Return false if processing
  // should not proceed.
  virtual bool RememberOriginalProperties() = 0;

  // Produce the definition. This must produce a definition.
  virtual CustomElementDefinition* Build(const CustomElementDescriptor&,
                                         CustomElementDefinition::Id) = 0;
};

}  // namespace blink

#endif  // CustomElementDefinitionBuilder_h
