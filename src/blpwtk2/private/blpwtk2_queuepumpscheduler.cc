/*
 * Copyright (C) 2019 Bloomberg Finance L.P.
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

#include <blpwtk2_queuepumpscheduler.h>

namespace blpwtk2 {

                        // ------------------------
                        // class QueuePumpScheduler
                        // ------------------------

// static
const std::string& QueuePumpScheduler::name()
{
    static std::string schedulerName = "queue";
    return schedulerName;
}

QueuePumpScheduler::QueuePumpScheduler()
{
    d_tunables.push_back({"timer-preemption-percent", 75});
}

QueuePumpScheduler::~QueuePumpScheduler()
{
}

void QueuePumpScheduler::isReadyToWork(bool *allowNormalWork, bool *allowIdleWork)
{
    // We will unintrusively keep our own message loop pumping without
    // preempting lower-priority messages.  We do this by first checking
    // what's on the Windows message queue.
    DWORD queueStatus =
        HIWORD(::GetQueueStatus(QS_INPUT | QS_SENDMESSAGE | QS_POSTMESSAGE | QS_TIMER | QS_PAINT));

    if (queueStatus & (QS_INPUT | QS_PAINT)) {
        // Don't allow chromium to work if there are input or paint messages
        return;
    }
    else if (queueStatus & QS_TIMER) {
        // Unlike WM_PAINT message (which is based on the display refresh
        // rate) and input message (which is based on user action), WM_TIMER
        // messages are added by windows in GetMessage or PeekMessage when the
        // queue is empty.  If we are always too polite about not inserting
        // any posted messages to not preempt low priority messages, Windows
        // would think that we are completely idle and will end up stuffing
        // the queue with more timer messages. To make sure Windows doesn't
        // confuse our niceness with idleness, we occasionally preempt the
        // timer message.

        unsigned timerPreemptionPercent = d_tunables[0].second;

        if ((++d_timerCount % 100) < timerPreemptionPercent) {
            *allowIdleWork = false;
            *allowNormalWork = true;
        }
    }
    else if (queueStatus & (QS_SENDMESSAGE | QS_POSTMESSAGE)) {
        // No low priority messages in the queue but we do have some
        // high-priority messages. We will schedule the pump but we'll
        // skip idle work.
        *allowIdleWork = false;
        *allowNormalWork = true;
    }
    else {
        // No messages are in the queue. We schedule the pump and also
        // do the idle work.
        *allowIdleWork = true;
        *allowNormalWork = true;
    }
}

std::vector<std::string> QueuePumpScheduler::listTunables()
{
    std::vector<std::string> tunables;

    for (const auto& tunable : d_tunables) {
        tunables.push_back(tunable.first);
    }

    return tunables;
}

int QueuePumpScheduler::setTunable(unsigned index, int value)
{
    if (index < d_tunables.size()) {
        d_tunables[index].second = value;
        return 0;
    }

    return -1;
}

}  // close namespace blpwtk2

// vim: ts=4 et
