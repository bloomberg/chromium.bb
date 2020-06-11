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

void BBWindowHooks::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  DOMWindowClient::Trace(visitor);
  Supplementable<BBWindowHooks>::Trace(visitor);
}

void BBWindowHooks::allowPrint(long value) {
    GetFrame()->AllowPrint(!!value);
}

} // namespace blink
