/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <base/no_destructor.h>
#include "third_party/blink/renderer/core/page/bb_window_hooks.h"

#include "third_party/blink/renderer/core/dom/character_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

static BBWindowHooks::PumpConfigHooks& GetPumpConfigHooks()
{
    static base::NoDestructor<BBWindowHooks::PumpConfigHooks> hooks;
    return *hooks;
}

BBWindowHooks::BBWindowHooks(LocalFrame* frame)
    : DOMWindowClient(frame)
{
}

// static
void BBWindowHooks::InstallPumpConfigHooks(PumpConfigHooks hooks)
{
    GetPumpConfigHooks() = hooks;
}

String BBWindowHooks::listPumpSchedulers() {
    std::vector<std::string> list = GetPumpConfigHooks().listSchedulers.Run();

    std::string combined_list;
    for (const auto& it : list) {
        if (combined_list.empty())
            combined_list = it;
        else
            combined_list += (", " + it);
    }

    return String::FromUTF8(combined_list.data(), combined_list.size());
}

String BBWindowHooks::listPumpSchedulerTunables() {
    std::vector<std::string> list = GetPumpConfigHooks().listSchedulerTunables.Run();

    std::string combined_list;
    for (const auto& it : list) {
        if (combined_list.empty())
            combined_list = it;
        else
            combined_list += (", " + it);
    }

    return String::FromUTF8(combined_list.data(), combined_list.size());
}

void BBWindowHooks::activatePumpScheduler(long index) {
    GetPumpConfigHooks().activateScheduler.Run(index);
}

void BBWindowHooks::setPumpSchedulerTunable(long index, long value) {
    GetPumpConfigHooks().setSchedulerTunable.Run(index, value);
}

bool BBWindowHooks::matchSelector(Node *node, const String& selector)
{
    if (node->IsElementNode() && !selector.IsEmpty()) {
        Element *e = ToElement(node);
        return e->matches(AtomicString(selector), IGNORE_EXCEPTION_FOR_TESTING);
    }
    return false;
}

void BBWindowHooks::appendTextContent(Node *node, StringBuilder& content,
                                      const String& excluder, const String& mask)
{
    if (matchSelector(node, excluder)) {
        content.Append(mask);
    } else if (node->getNodeType() == Node::kTextNode) {
        content.Append((static_cast<CharacterData*>(node))->data());
    } else {
        if (node->HasTagName(html_names::kBrTag)) {
            content.Append('\n');
        } else {
            for (Node* child = node->firstChild(); child;
                child = child->nextSibling()) {
                    appendTextContent(child, content, excluder, mask);
                    if (!matchSelector(child, excluder) && isBlock(child) &&
                        !(child->HasTagName(html_names::kTdTag) ||
                        child->HasTagName(html_names::kThTag))) {
                            content.Append('\n');
                    } else if (child->nextSibling()) {
                        if (!matchSelector(child->nextSibling(), excluder)) {
                            if (child->HasTagName(html_names::kTdTag) ||
                                child->HasTagName(html_names::kThTag)) {
                                    content.Append('\t');
                            } else if (child->HasTagName(html_names::kTrTag)
                                || isBlock(child->nextSibling())) {
                                    content.Append('\n');
                            }
                        }
                    }
            }
        }
    }
}

bool BBWindowHooks::isBlock(Node* node)
{
    return blink::IsEnclosingBlock(node);
}

String BBWindowHooks::getPlainText(Node* node, const String& excluder, const String& mask)
{
    StringBuilder content;
    appendTextContent(node, content, excluder, mask);
    return content.ToString();
}

void BBWindowHooks::setTextMatchMarkerVisibility(Document* document, bool highlight)
{
    LocalFrame *frame = document->GetFrame();
    frame->GetEditor().SetMarkedTextMatchesAreHighlighted(highlight);
}

bool BBWindowHooks::checkSpellingForRange(Range* range)
{
    Node* ancestor = range->commonAncestorContainer();

    if (!ancestor) {
        return false;
    }

    LocalFrame *frame = range->OwnerDocument().GetFrame();
    // VisibleSelection s(range);
    frame->GetSpellChecker().ReplaceMisspelledRange(range->toString());
    // frame->GetSpellChecker().MarkMisspellingsAndBadGrammar(s, false, s);
    return true;
}

void BBWindowHooks::removeMarker(Range* range, long mask)
{
    range->OwnerDocument().Markers().RemoveMarkersInRange(
        EphemeralRange(range),
        DocumentMarker::MarkerTypes(mask));
}

void BBWindowHooks::addHighlightMarker(Range* range, long foregroundColor, long backgroundColor, bool includeNonSelectableText)
{
    range->OwnerDocument().Markers().AddHighlightMarker(
        EphemeralRange(range),
        Color(foregroundColor),
        Color(backgroundColor).BlendWithWhite(),
        includeNonSelectableText);
}

void BBWindowHooks::removeHighlightMarker(Range *range)
{
    removeMarker(range, DocumentMarker::kHighlight);
}

Range* BBWindowHooks::findPlainText(Range* range, const String& target, long options)
{
    EphemeralRangeInFlatTree result_range =
        blink::FindBuffer::FindMatchInRange(EphemeralRangeInFlatTree(range), target, options);

    Range* range_object =
        Range::Create(result_range.GetDocument(),
                      ToPositionInDOMTree(result_range.StartPosition()),
                      ToPositionInDOMTree(result_range.EndPosition()));
    return range_object;
}

bool BBWindowHooks::checkSpellingForNode(Node* node)
{
    if (!node->IsElementNode()) {
        return false;
    }

    Element* e = ToElement(node);

    if (e && e->IsSpellCheckingEnabled()) {
        LocalFrame *frame = e->GetDocument().GetFrame();
        if (frame) {
            // VisibleSelection s(
            //     FirstPositionInOrBeforeNode(e),
            //     LastPositionInOrAfterNode(e));
            frame->GetSpellChecker().ReplaceMisspelledRange(Range::Create(e->GetDocument())->toString());
            // frame->GetSpellChecker().MarkMisspellingsAndBadGrammar(s, false, s);
        }
        return true;
    }
    return false;
}

DOMRectReadOnly* BBWindowHooks::getAbsoluteCaretRectAtOffset(Node* node, long offset)
{
    VisiblePosition visiblePos = CreateVisiblePosition(Position(node, offset));
    IntRect rc = AbsoluteCaretBoundsOf(visiblePos.ToPositionWithAffinity());
    return DOMRectReadOnly::FromIntRect(rc);
}

bool BBWindowHooks::isOverwriteModeEnabled(Document* document)
{
    LocalFrame *frame = document->GetFrame();
    return frame->GetEditor().IsOverwriteModeEnabled();
}

void BBWindowHooks::toggleOverwriteMode(Document* document)
{
    document->GetFrame()->GetEditor().ToggleOverwriteModeEnabled();
}

void BBWindowHooks::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  DOMWindowClient::Trace(visitor);
  Supplementable<BBWindowHooks>::Trace(visitor);
}

} // namespace blink
