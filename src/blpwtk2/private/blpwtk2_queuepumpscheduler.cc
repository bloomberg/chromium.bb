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
    d_tunables.push_back({"budget", 80});
}

QueuePumpScheduler::~QueuePumpScheduler()
{
}

void QueuePumpScheduler::isReadyToWork(bool *allowNormalWork, bool *allowIdleWork)
{
    DWORD queueStatus =
        HIWORD(::GetQueueStatus(QS_SENDMESSAGE | QS_POSTMESSAGE | QS_TIMER | QS_PAINT));

    if (GetInputState() || queueStatus & (QS_TIMER | QS_PAINT)) {
        // The input messages are only fetched from the input queue when
        // GetMessage/PeekMessage does not find any posted messages in the
        // post message queue. Similarly, paint and timer messages are
        // synthesized based on a flag in the window only if both the post
        // message queue and the input message queue are empty.
        //
        // Here are some example scenarios that can cause a low priority
        // message to always be available:
        //
        // input message: The user can trigger frequent WM_MOUSEMOVE events
        //   by moving the mouse cursor over the window.
        //
        // paint message: The application calls InvalidateRect on every event
        //   loop cycle and/or the application does not fully validate the
        //   damaged rect when processing the WM_PAINT message.
        //
        // timer message: SetTimer is called with a period of 10ms or less.
        //   If the main event loop takes more than 10ms to run a single
        //   cycle, the timer flag will always be set for the window.
        //
        // Since the emptiness of the post message queue causes
        // GetMessage/PeekMessage to dispatch these low priority messages and
        // the presence of these low priority messages prevent us from posting
        // the chrome pump message, it's possible for the thread to enter a
        // state where the chrome pump message is starved by these low
        // priority  messages.  For this reason, we only preempt ourselves a
        // fraction of the time.
        d_totalBudget += d_tunables[0].second;

        if (d_totalBudget >= 100) {
            d_totalBudget -= 100;
            *allowNormalWork = true;
        }
    }
    else {
        // Reset the budget for our pump message.  The budget is only meant to
        // allow preemption of a long chain of low priority messages to allow
        // chrome to do some work but we don't want to use it to preempt a new
        // chain of low priority messages.
        d_totalBudget = 0;

        // Allow chrome to work.
        *allowNormalWork = true;

        if (0 == (queueStatus & (QS_SENDMESSAGE | QS_POSTMESSAGE))) {
            // No messages are in the queue. We allow chrome to do idle work
            // as well.
            *allowIdleWork = true;
        }
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
