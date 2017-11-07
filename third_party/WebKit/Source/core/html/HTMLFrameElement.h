/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004, 2006, 2009 Apple Inc. All rights reserved.
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

#ifndef HTMLFrameElement_h
#define HTMLFrameElement_h

#include "core/CoreExport.h"
#include "core/html/HTMLFrameElementBase.h"
#include "third_party/WebKit/common/feature_policy/feature_policy.h"

namespace blink {

class CORE_EXPORT HTMLFrameElement final : public HTMLFrameElementBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DECLARE_NODE_FACTORY(HTMLFrameElement);

  bool HasFrameBorder() const { return frame_border_; }

  bool NoResize() const;

  ParsedFeaturePolicy ConstructContainerPolicy(
      Vector<String>* /* messages */,
      bool* /* old_syntax */) const override;

 private:
  explicit HTMLFrameElement(Document&);

  void AttachLayoutTree(AttachContext&) override;

  bool LayoutObjectIsNeeded(const ComputedStyle&) override;
  LayoutObject* CreateLayoutObject(const ComputedStyle&) override;

  void ParseAttribute(const AttributeModificationParams&) override;

  bool frame_border_;
  bool frame_border_set_;
};

}  // namespace blink

#endif  // HTMLFrameElement_h
