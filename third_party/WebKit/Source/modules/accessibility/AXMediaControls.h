/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AXMediaControls_h
#define AXMediaControls_h

#include "base/macros.h"
#include "modules/accessibility/AXSlider.h"
#include "modules/media_controls/elements/MediaControlElementType.h"

namespace blink {

class AXObjectCacheImpl;

class AccessibilityMediaControl : public AXLayoutObject {
 public:
  static AXObject* Create(LayoutObject*, AXObjectCacheImpl&);
  ~AccessibilityMediaControl() override = default;

  AccessibilityRole RoleValue() const override;

  String TextAlternative(bool recursive,
                         bool in_aria_labelled_by_traversal,
                         AXObjectSet& visited,
                         AXNameFrom&,
                         AXRelatedObjectVector*,
                         NameSources*) const override;
  String Description(AXNameFrom,
                     AXDescriptionFrom&,
                     AXObjectVector* description_objects) const override;

 protected:
  AccessibilityMediaControl(LayoutObject*, AXObjectCacheImpl&);
  MediaControlElementType ControlType() const;
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityMediaControl);
};

class AccessibilityMediaTimeline final : public AXSlider {
 public:
  static AXObject* Create(LayoutObject*, AXObjectCacheImpl&);
  ~AccessibilityMediaTimeline() override = default;

  String Description(AXNameFrom,
                     AXDescriptionFrom&,
                     AXObjectVector* description_objects) const override;
  String ValueDescription() const override;

 private:
  AccessibilityMediaTimeline(LayoutObject*, AXObjectCacheImpl&);

  DISALLOW_COPY_AND_ASSIGN(AccessibilityMediaTimeline);
};

class AXMediaControlsContainer final : public AccessibilityMediaControl {
 public:
  static AXObject* Create(LayoutObject*, AXObjectCacheImpl&);
  ~AXMediaControlsContainer() override = default;

  AccessibilityRole RoleValue() const override { return kToolbarRole; }

  String TextAlternative(bool recursive,
                         bool in_aria_labelled_by_traversal,
                         AXObjectSet& visited,
                         AXNameFrom&,
                         AXRelatedObjectVector*,
                         NameSources*) const override;
  String Description(AXNameFrom,
                     AXDescriptionFrom&,
                     AXObjectVector* description_objects) const override;

 private:
  AXMediaControlsContainer(LayoutObject*, AXObjectCacheImpl&);
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;

  DISALLOW_COPY_AND_ASSIGN(AXMediaControlsContainer);
};

class AccessibilityMediaTimeDisplay final : public AccessibilityMediaControl {
 public:
  static AXObject* Create(LayoutObject*, AXObjectCacheImpl&);
  ~AccessibilityMediaTimeDisplay() override = default;

  AccessibilityRole RoleValue() const override { return kStaticTextRole; }

  String StringValue() const override;
  String TextAlternative(bool recursive,
                         bool in_aria_labelled_by_traversal,
                         AXObjectSet& visited,
                         AXNameFrom&,
                         AXRelatedObjectVector*,
                         NameSources*) const override;

 private:
  AccessibilityMediaTimeDisplay(LayoutObject*, AXObjectCacheImpl&);
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityMediaTimeDisplay);
};

}  // namespace blink

#endif  // AXMediaControls_h
