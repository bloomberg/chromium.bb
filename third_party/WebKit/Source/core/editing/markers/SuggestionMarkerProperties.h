// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SuggestionMarkerProperties_h
#define SuggestionMarkerProperties_h

#include "core/CoreExport.h"
#include "core/editing/markers/StyleableMarker.h"
#include "core/editing/markers/SuggestionMarker.h"

namespace blink {

// This class is used to pass parameters to
// |DocumentMarkerController::AddSuggestionMarker()|.
class CORE_EXPORT SuggestionMarkerProperties final {
  STACK_ALLOCATED();

 public:
  class CORE_EXPORT Builder;

  SuggestionMarkerProperties(const SuggestionMarkerProperties&);
  SuggestionMarkerProperties();

  SuggestionMarker::SuggestionType Type() const { return type_; }
  Vector<String> Suggestions() const { return suggestions_; }
  Color HighlightColor() const { return highlight_color_; }
  Color UnderlineColor() const { return underline_color_; }
  Color BackgroundColor() const { return background_color_; }
  StyleableMarker::Thickness Thickness() const { return thickness_; }

 private:
  SuggestionMarker::SuggestionType type_ =
      SuggestionMarker::SuggestionType::kNotMisspelling;
  Vector<String> suggestions_;
  Color highlight_color_ = Color::kTransparent;
  Color underline_color_ = Color::kTransparent;
  Color background_color_ = Color::kTransparent;
  StyleableMarker::Thickness thickness_ = StyleableMarker::Thickness::kThin;
};

// This class is used for building SuggestionMarkerProperties objects.
class CORE_EXPORT SuggestionMarkerProperties::Builder final {
  STACK_ALLOCATED();

 public:
  explicit Builder(const SuggestionMarkerProperties&);
  Builder();

  SuggestionMarkerProperties Build() const;

  Builder& SetType(SuggestionMarker::SuggestionType);
  Builder& SetSuggestions(const Vector<String>& suggestions);
  Builder& SetHighlightColor(Color);
  Builder& SetUnderlineColor(Color);
  Builder& SetBackgroundColor(Color);
  Builder& SetThickness(StyleableMarker::Thickness);

 private:
  SuggestionMarkerProperties data_;

  DISALLOW_COPY_AND_ASSIGN(Builder);
};

}  // namespace blink

#endif  // SuggestionMarkerProperties_h
