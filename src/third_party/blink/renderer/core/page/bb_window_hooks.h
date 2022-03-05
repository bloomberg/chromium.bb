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


#ifndef BBWindowHooks_h
#define BBWindowHooks_h

#include "base/callback.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

    class LocalFrame;
    class Node;
    class Document;
    class Range;

    class BBWindowHooks final : public ScriptWrappable,
                                public ExecutionContextClient,
                                public Supplementable<BBWindowHooks>  {
        DEFINE_WRAPPERTYPEINFO();
    public:
        struct PumpConfigHooks {
            base::RepeatingCallback<std::vector<std::string>(void)> listSchedulers;
            base::RepeatingCallback<std::vector<std::string>(void)> listSchedulerTunables;
            base::RepeatingCallback<int(int)> activateScheduler;
            base::RepeatingCallback<int(unsigned,int)> setSchedulerTunable;
        };

        static BBWindowHooks* Create(LocalDOMWindow* window) { return MakeGarbageCollected<BBWindowHooks>(window); }
        BLINK_EXPORT static void InstallPumpConfigHooks(PumpConfigHooks hooks);

        String listPumpSchedulers();
        String listPumpSchedulerTunables();
        void activatePumpScheduler(long index);
        void setPumpSchedulerTunable(long index, long value);


        // patch section: document marker highlightmarker
        bool isBlock(Node* node);
        String getPlainText(Node* node, const String& excluder = "", const String& mask = "");
        void setTextMatchMarkerVisibility(Document* document, bool highlight);
        bool checkSpellingForRange(Range* range);
        void removeMarker(Range* range, long mask);
        void addHighlightMarker(Range* range, long foregroundColor, long backgroundColor, bool includeNonSelectableText = false);
        void removeHighlightMarker(Range *range);
        Range* findPlainText(Range* range, const String& target, long options);
        bool checkSpellingForNode(Node* node);
        DOMRectReadOnly* getAbsoluteCaretRectAtOffset(Node* node, long offset);



        // patch section: docprinter



        // patch section: dump diagnostics



        void Trace(Visitor*) const override;

        explicit BBWindowHooks(LocalDOMWindow *window);
    private:

        bool matchSelector(Node *node, const String& selector);
        void appendTextContent(Node *node, StringBuilder& content, const String& excluder, const String& mask);
    };

} // namespace blink

#endif // BBWindowHooks_h
