/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
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
 *
 */

#ifndef XSLImportRule_h
#define XSLImportRule_h

#include "core/xml/XSLStyleSheet.h"
#include "platform/runtime_enabled_features.h"

namespace blink {

class XSLImportRule final : public GarbageCollectedFinalized<XSLImportRule> {
 public:
  static XSLImportRule* Create(XSLStyleSheet* parent_sheet,
                               const String& href) {
    DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
    return new XSLImportRule(parent_sheet, href);
  }

  virtual ~XSLImportRule();
  virtual void Trace(blink::Visitor*);

  const String& Href() const { return str_href_; }
  XSLStyleSheet* GetStyleSheet() const { return style_sheet_.Get(); }

  XSLStyleSheet* ParentStyleSheet() const { return parent_style_sheet_; }
  void SetParentStyleSheet(XSLStyleSheet* style_sheet) {
    parent_style_sheet_ = style_sheet;
  }

  bool IsLoading();
  void LoadSheet();

 private:
  XSLImportRule(XSLStyleSheet* parent_sheet, const String& href);

  void SetXSLStyleSheet(const String& href,
                        const KURL& base_url,
                        const String& sheet);

  Member<XSLStyleSheet> parent_style_sheet_;
  String str_href_;
  Member<XSLStyleSheet> style_sheet_;
  bool loading_;
};

}  // namespace blink

#endif  // XSLImportRule_h
