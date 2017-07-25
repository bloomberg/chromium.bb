/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SpellChecker_h
#define SpellChecker_h

#include "core/CoreExport.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/markers/DocumentMarker.h"
#include "platform/heap/Handle.h"
#include "platform/text/TextChecking.h"

namespace blink {

class CompositeEditCommand;
class IdleSpellCheckCallback;
class LocalFrame;
class ReplaceSelectionCommand;
class SpellCheckerClient;
class SpellCheckMarker;
class SpellCheckRequest;
class SpellCheckRequester;
class TextCheckerClient;
class TextCheckingParagraph;
struct TextCheckingResult;
class TypingCommand;
enum class TypingContinuation;
class WebSpellCheckPanelHostClient;

class CORE_EXPORT SpellChecker final : public GarbageCollected<SpellChecker> {
  WTF_MAKE_NONCOPYABLE(SpellChecker);

 public:
  static SpellChecker* Create(LocalFrame&);

  DECLARE_TRACE();

  SpellCheckerClient& GetSpellCheckerClient() const;
  WebSpellCheckPanelHostClient& SpellCheckPanelHostClient() const;
  TextCheckerClient& TextChecker() const;

  static bool IsSpellCheckingEnabledAt(const Position&);
  bool IsSpellCheckingEnabled() const;
  void ToggleSpellCheckingEnabled();
  void IgnoreSpelling();
  bool IsSpellCheckingEnabledInFocusedNode() const;
  void MarkMisspellingsAfterApplyingCommand(const CompositeEditCommand&);
  void MarkAndReplaceFor(SpellCheckRequest*, const Vector<TextCheckingResult>&);
  void AdvanceToNextMisspelling(bool start_before_selection);
  void ShowSpellingGuessPanel();
  void DidBeginEditing(Element*);
  void MarkMisspellingsForMovingParagraphs(const VisibleSelection&);
  void RespondToChangedContents();
  void RespondToChangedSelection(const Position& old_selection_start,
                                 TypingContinuation);
  std::pair<Node*, SpellCheckMarker*> GetSpellCheckMarkerUnderSelection() const;
  // The first String returned in the pair is the selected text.
  // The second String is the marker's description.
  std::pair<String, String> SelectMisspellingAsync();
  void ReplaceMisspelledRange(const String&);
  void RemoveSpellingMarkers();
  void RemoveSpellingMarkersUnderWords(const Vector<String>& words);
  enum class ElementsType { kAll, kOnlyNonEditable };
  void RemoveSpellingAndGrammarMarkers(const HTMLElement&,
                                       ElementsType = ElementsType::kAll);
  void SpellCheckAfterBlur();

  void DidEndEditingOnTextField(Element*);
  bool SelectionStartHasMarkerFor(DocumentMarker::MarkerType,
                                  int from,
                                  int length) const;
  void UpdateMarkersForWordsAffectedByEditing(
      bool only_handle_words_containing_selection);
  void CancelCheck();

  // Exposed for testing and idle time spell checker
  SpellCheckRequester& GetSpellCheckRequester() const {
    return *spell_check_requester_;
  }
  IdleSpellCheckCallback& GetIdleSpellCheckCallback() const {
    return *idle_spell_check_callback_;
  }

  // The leak detector will report leaks should queued requests be posted
  // while it GCs repeatedly, as the requests keep their associated element
  // alive.
  //
  // Hence allow the leak detector to effectively stop the spell checker to
  // ensure leak reporting stability.
  void PrepareForLeakDetection();

  void DocumentAttached(Document*);

 private:
  explicit SpellChecker(LocalFrame&);

  LocalFrame& GetFrame() const {
    DCHECK(frame_);
    return *frame_;
  }

  // Helper functions for advanceToNextMisspelling()
  Vector<TextCheckingResult> FindMisspellings(const String&);
  std::pair<String, int> FindFirstMisspelling(const Position&, const Position&);

  void MarkMisspellingsAfterLineBreak(const VisibleSelection& word_selection);
  void MarkMisspellingsAfterTypingToWord(const VisiblePosition& word_start);
  void MarkMisspellingsAfterTypingCommand(const TypingCommand&);
  void MarkMisspellingsAfterReplaceSelectionCommand(
      const ReplaceSelectionCommand&);

  void RemoveMarkers(const EphemeralRange&, DocumentMarker::MarkerTypes);

  void MarkMisspellingsInternal(const VisibleSelection&);
  void ChunkAndMarkAllMisspellings(
      const TextCheckingParagraph& full_paragraph_to_check);
  void SpellCheckOldSelection(const Position& old_selection_start,
                              const VisibleSelection& new_adjacent_words);

  Member<LocalFrame> frame_;

  const Member<SpellCheckRequester> spell_check_requester_;
  const Member<IdleSpellCheckCallback> idle_spell_check_callback_;
};

}  // namespace blink

#endif  // SpellChecker_h
