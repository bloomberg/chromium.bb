/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef SpellCheckerClientImpl_h
#define SpellCheckerClientImpl_h

#include "core/CoreExport.h"
#include "core/editing/spellcheck/SpellCheckerClient.h"

namespace blink {

class WebViewImpl;

class CORE_EXPORT SpellCheckerClientImpl final : public SpellCheckerClient {
 public:
  explicit SpellCheckerClientImpl(WebViewImpl*);
  ~SpellCheckerClientImpl() override;

  bool IsSpellCheckingEnabled() override;
  void ToggleSpellCheckingEnabled() override;

 private:
  // Returns whether or not the focused control needs spell-checking.
  // Currently, this function just retrieves the focused node and determines
  // whether or not it is a <textarea> element or an element whose
  // contenteditable attribute is true.
  // FIXME: Bug 740540: This code just implements the default behavior
  // proposed in this issue. We should also retrieve "spellcheck" attributes
  // for text fields and create a flag to over-write the default behavior.
  bool ShouldSpellcheckByDefault();

  WebViewImpl* web_view_;

  // This flag is set to false if spell check for this editor is manually
  // turned off. The default setting is SpellCheckAutomatic.
  enum {
    kSpellCheckAutomatic,
    kSpellCheckForcedOn,
    kSpellCheckForcedOff
  } spell_check_this_field_status_;
};

}  // namespace blink

#endif
