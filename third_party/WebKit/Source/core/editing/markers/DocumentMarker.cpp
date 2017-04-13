/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/editing/markers/DocumentMarker.h"

#include "wtf/StdLibExtras.h"

namespace blink {

DocumentMarkerDetails::~DocumentMarkerDetails() {}

class DocumentMarkerDescription final : public DocumentMarkerDetails {
 public:
  static DocumentMarkerDescription* Create(const String&);

  const String& Description() const { return description_; }
  bool IsDescription() const override { return true; }

 private:
  explicit DocumentMarkerDescription(const String& description)
      : description_(description) {}

  String description_;
};

DocumentMarkerDescription* DocumentMarkerDescription::Create(
    const String& description) {
  return new DocumentMarkerDescription(description);
}

inline DocumentMarkerDescription* ToDocumentMarkerDescription(
    DocumentMarkerDetails* details) {
  if (details && details->IsDescription())
    return static_cast<DocumentMarkerDescription*>(details);
  return 0;
}

class DocumentMarkerTextMatch final : public DocumentMarkerDetails {
 public:
  static DocumentMarkerTextMatch* Create(DocumentMarker::MatchStatus);

  bool IsActiveMatch() const {
    return match_status_ == DocumentMarker::MatchStatus::kActive;
  }

  bool IsTextMatch() const override { return true; }

 private:
  explicit DocumentMarkerTextMatch(DocumentMarker::MatchStatus match_status)
      : match_status_(match_status) {}

  DocumentMarker::MatchStatus match_status_;
};

DocumentMarkerTextMatch* DocumentMarkerTextMatch::Create(
    DocumentMarker::MatchStatus match_status) {
  DEFINE_STATIC_LOCAL(
      DocumentMarkerTextMatch, active_instance,
      (new DocumentMarkerTextMatch(DocumentMarker::MatchStatus::kActive)));
  DEFINE_STATIC_LOCAL(
      DocumentMarkerTextMatch, inactive_instance,
      (new DocumentMarkerTextMatch(DocumentMarker::MatchStatus::kInactive)));
  return match_status == DocumentMarker::MatchStatus::kActive
             ? &active_instance
             : &inactive_instance;
}

inline DocumentMarkerTextMatch* ToDocumentMarkerTextMatch(
    DocumentMarkerDetails* details) {
  if (details && details->IsTextMatch())
    return static_cast<DocumentMarkerTextMatch*>(details);
  return 0;
}

class TextCompositionMarkerDetails final : public DocumentMarkerDetails {
 public:
  static TextCompositionMarkerDetails* Create(Color underline_color,
                                              bool thick,
                                              Color background_color);

  bool IsComposition() const override { return true; }
  Color UnderlineColor() const { return underline_color_; }
  bool Thick() const { return thick_; }
  Color BackgroundColor() const { return background_color_; }

 private:
  TextCompositionMarkerDetails(Color underline_color,
                               bool thick,
                               Color background_color)
      : underline_color_(underline_color),
        background_color_(background_color),
        thick_(thick) {}

  Color underline_color_;
  Color background_color_;
  bool thick_;
};

TextCompositionMarkerDetails* TextCompositionMarkerDetails::Create(
    Color underline_color,
    bool thick,
    Color background_color) {
  return new TextCompositionMarkerDetails(underline_color, thick,
                                          background_color);
}

inline TextCompositionMarkerDetails* ToTextCompositionMarkerDetails(
    DocumentMarkerDetails* details) {
  if (details && details->IsComposition())
    return static_cast<TextCompositionMarkerDetails*>(details);
  return nullptr;
}

DocumentMarker::DocumentMarker(MarkerType type,
                               unsigned start_offset,
                               unsigned end_offset,
                               const String& description)
    : type_(type),
      start_offset_(start_offset),
      end_offset_(end_offset),
      details_(description.IsEmpty()
                   ? nullptr
                   : DocumentMarkerDescription::Create(description)) {}

DocumentMarker::DocumentMarker(unsigned start_offset,
                               unsigned end_offset,
                               DocumentMarker::MatchStatus match_status)
    : type_(DocumentMarker::kTextMatch),
      start_offset_(start_offset),
      end_offset_(end_offset),
      details_(DocumentMarkerTextMatch::Create(match_status)) {}

DocumentMarker::DocumentMarker(unsigned start_offset,
                               unsigned end_offset,
                               Color underline_color,
                               bool thick,
                               Color background_color)
    : type_(DocumentMarker::kComposition),
      start_offset_(start_offset),
      end_offset_(end_offset),
      details_(TextCompositionMarkerDetails::Create(underline_color,
                                                    thick,
                                                    background_color)) {}

DocumentMarker::DocumentMarker(const DocumentMarker& marker)
    : type_(marker.GetType()),
      start_offset_(marker.StartOffset()),
      end_offset_(marker.EndOffset()),
      details_(marker.Details()) {}

Optional<DocumentMarker::MarkerOffsets>
DocumentMarker::ComputeOffsetsAfterShift(unsigned offset,
                                         unsigned old_length,
                                         unsigned new_length) const {
  MarkerOffsets result;
  result.start_offset = StartOffset();
  result.end_offset = EndOffset();

  // algorithm inspired by https://dom.spec.whatwg.org/#concept-cd-replace
  // but with some changes

  // Deviation from the concept-cd-replace algorithm: second condition in the
  // next line (don't include text inserted immediately before a marker in the
  // marked range, but do include the new text if it's replacing text in the
  // marked range)
  if (StartOffset() > offset || (StartOffset() == offset && old_length == 0)) {
    if (StartOffset() <= offset + old_length) {
      // Marker start was in the replaced text. Move to end of new text
      // (Deviation from the concept-cd-replace algorithm: that algorithm
      // would move to the beginning of the new text here)
      result.start_offset = offset + new_length;
    } else {
      // Marker start was after the replaced text. Shift by length
      // difference
      result.start_offset = StartOffset() + new_length - old_length;
    }
  }

  if (EndOffset() > offset) {
    // Deviation from the concept-cd-replace algorithm: < instead of <= in
    // the next line
    if (EndOffset() < offset + old_length) {
      // Marker end was in the replaced text. Move to beginning of new text
      result.end_offset = offset;
    } else {
      // Marker end was after the replaced text. Shift by length difference
      result.end_offset = EndOffset() + new_length - old_length;
    }
  }

  if (result.start_offset >= result.end_offset)
    return WTF::kNullopt;

  return result;
}

void DocumentMarker::ShiftOffsets(int delta) {
  start_offset_ += delta;
  end_offset_ += delta;
}

void DocumentMarker::SetIsActiveMatch(bool active) {
  details_ = DocumentMarkerTextMatch::Create(
      active ? DocumentMarker::MatchStatus::kActive
             : DocumentMarker::MatchStatus::kInactive);
}

const String& DocumentMarker::Description() const {
  if (DocumentMarkerDescription* details =
          ToDocumentMarkerDescription(details_.Get()))
    return details->Description();
  return g_empty_string;
}

bool DocumentMarker::IsActiveMatch() const {
  if (DocumentMarkerTextMatch* details =
          ToDocumentMarkerTextMatch(details_.Get()))
    return details->IsActiveMatch();
  return false;
}

Color DocumentMarker::UnderlineColor() const {
  if (TextCompositionMarkerDetails* details =
          ToTextCompositionMarkerDetails(details_.Get()))
    return details->UnderlineColor();
  return Color::kTransparent;
}

bool DocumentMarker::Thick() const {
  if (TextCompositionMarkerDetails* details =
          ToTextCompositionMarkerDetails(details_.Get()))
    return details->Thick();
  return false;
}

Color DocumentMarker::BackgroundColor() const {
  if (TextCompositionMarkerDetails* details =
          ToTextCompositionMarkerDetails(details_.Get()))
    return details->BackgroundColor();
  return Color::kTransparent;
}

DEFINE_TRACE(DocumentMarker) {
  visitor->Trace(details_);
}

}  // namespace blink
