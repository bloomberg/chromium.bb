/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef CSSStyleDeclaration_h
#define CSSStyleDeclaration_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CSSRule;
class CSSStyleSheet;
class CSSValue;
class ExceptionState;

class CORE_EXPORT CSSStyleDeclaration
    : public GarbageCollectedFinalized<CSSStyleDeclaration>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(CSSStyleDeclaration);

 public:
  virtual ~CSSStyleDeclaration() {}

  virtual CSSRule* parentRule() const = 0;
  String cssFloat() { return GetPropertyValueInternal(CSSPropertyFloat); }
  void setCSSFloat(const String& value, ExceptionState& exception_state) {
    SetPropertyInternal(CSSPropertyFloat, String(), value, false,
                        exception_state);
  }
  virtual String cssText() const = 0;
  virtual void setCSSText(const String&, ExceptionState&) = 0;
  virtual unsigned length() const = 0;
  virtual String item(unsigned index) const = 0;
  virtual String getPropertyValue(const String& property_name) = 0;
  virtual String getPropertyPriority(const String& property_name) = 0;
  virtual String GetPropertyShorthand(const String& property_name) = 0;
  virtual bool IsPropertyImplicit(const String& property_name) = 0;
  virtual void setProperty(const String& property_name,
                           const String& value,
                           const String& priority,
                           ExceptionState&) = 0;
  virtual String removeProperty(const String& property_name,
                                ExceptionState&) = 0;

  // CSSPropertyID versions of the CSSOM functions to support bindings and
  // editing.
  // Use the non-virtual methods in the concrete subclasses when possible.
  // The CSSValue returned by this function should not be exposed to the web as
  // it may be used by multiple documents at the same time.
  virtual const CSSValue* GetPropertyCSSValueInternal(CSSPropertyID) = 0;
  virtual const CSSValue* GetPropertyCSSValueInternal(
      AtomicString custom_property_name) = 0;
  virtual String GetPropertyValueInternal(CSSPropertyID) = 0;
  virtual void SetPropertyInternal(CSSPropertyID,
                                   const String& property_value,
                                   const String& value,
                                   bool important,
                                   ExceptionState&) = 0;

  virtual bool CssPropertyMatches(CSSPropertyID, const CSSValue*) const = 0;
  virtual CSSStyleSheet* ParentStyleSheet() const { return 0; }

  DEFINE_INLINE_VIRTUAL_TRACE() {}

 protected:
  CSSStyleDeclaration() {}
};

}  // namespace blink

#endif  // CSSStyleDeclaration_h
