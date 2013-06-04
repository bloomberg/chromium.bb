/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "config.h"
#include "core/editing/SpellingCorrectionCommand.h"

#include "core/dom/Document.h"
#include "core/editing/InsertTextCommand.h"
#include "core/editing/SetSelectionCommand.h"
#include "core/editing/TextIterator.h"
#include "core/page/Frame.h"

namespace WebCore {

SpellingCorrectionCommand::SpellingCorrectionCommand(PassRefPtr<Range> rangeToBeCorrected, const String& correction)
    : CompositeEditCommand(rangeToBeCorrected->startContainer()->document())
    , m_rangeToBeCorrected(rangeToBeCorrected)
    , m_selectionToBeCorrected(m_rangeToBeCorrected.get())
    , m_correction(correction)
{
}

void SpellingCorrectionCommand::doApply()
{
    m_corrected = plainText(m_rangeToBeCorrected.get());
    if (!m_corrected.length())
        return;

    if (!document()->frame()->selection()->shouldChangeSelection(m_selectionToBeCorrected))
        return;

    applyCommandToComposite(SetSelectionCommand::create(m_selectionToBeCorrected, FrameSelection::SpellCorrectionTriggered | FrameSelection::CloseTyping | FrameSelection::ClearTypingStyle));
    applyCommandToComposite(InsertTextCommand::create(document(), m_correction));
}

bool SpellingCorrectionCommand::shouldRetainAutocorrectionIndicator() const
{
    return true;
}

} // namespace WebCore
