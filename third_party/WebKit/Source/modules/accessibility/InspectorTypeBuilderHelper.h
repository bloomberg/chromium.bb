// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorTypeBuilderHelper_h
#define InspectorTypeBuilderHelper_h

#include "modules/ModulesExport.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "platform/inspector_protocol/TypeBuilder.h"

namespace blink {

using namespace protocol::Accessibility;

PassOwnPtr<AXProperty> createProperty(const String& name, PassOwnPtr<AXValue>);
PassOwnPtr<AXProperty> createProperty(IgnoredReason);

PassOwnPtr<AXValue> createValue(const String& value, const String& type = AXValueTypeEnum::String);
PassOwnPtr<AXValue> createValue(int value, const String& type = AXValueTypeEnum::Integer);
PassOwnPtr<AXValue> createValue(float value, const String& valueType = AXValueTypeEnum::Number);
PassOwnPtr<AXValue> createBooleanValue(bool value, const String& valueType = AXValueTypeEnum::Boolean);
PassOwnPtr<AXValue> createRelatedNodeListValue(const AXObject*, String* name = nullptr, const String& valueType = AXValueTypeEnum::Idref);
PassOwnPtr<AXValue> createRelatedNodeListValue(AXRelatedObjectVector&, const String& valueType);
PassOwnPtr<AXValue> createRelatedNodeListValue(AXObject::AXObjectVector& axObjects, const String& valueType = AXValueTypeEnum::IdrefList);

PassOwnPtr<AXValueSource> createValueSource(NameSource&);

} // namespace blink

#endif // InspectorAccessibilityAgent_h
