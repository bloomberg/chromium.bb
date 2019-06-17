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
    d_tunables.push_back({"important-work-budget", 80});
    d_tunables.push_back({"idle-work-budget", 50});
    d_tunables.push_back({"important-work-charge", 100});
    d_tunables.push_back({"idle-work-charge", 120});
}

QueuePumpScheduler::~QueuePumpScheduler()
{
}

void QueuePumpScheduler::isReadyToWork(bool *allowNormalWork, bool *allowIdleWork)
{
    DWORD queueStatus =
        HIWORD(::GetQueueStatus(QS_SENDMESSAGE | QS_TIMER | QS_PAINT));

    unsigned int importantWorkBudget = d_tunables[0].second;
    unsigned int idleWorkBudget = d_tunables[1].second;
    unsigned int importantWorkCharge = d_tunables[2].second;
    unsigned int idleWorkCharge = d_tunables[3].second;

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
        d_importantWorkBudget += importantWorkBudget;
        d_idleWorkBudget += idleWorkBudget;
    }
    else {
        // Since there are no pending low-priority messages, we
        // conditionally allow chromium to do work by adding the charge
        // to the budget.
        d_importantWorkBudget += importantWorkCharge;

        if (0 == (queueStatus & QS_SENDMESSAGE)) {
            // No sent messages are in the Windows queue so we allow Chromium
            // to do idle work as well.  This is similar to the upstream
            // implementation of MessagePumpForUI::DoRunLoop(), where we don't
            // call DoIdleWork() if ProcessNextWindowMessage() returns true.
            d_idleWorkBudget += idleWorkCharge;
        }
    }

    if (d_importantWorkBudget >= importantWorkCharge) {
        d_importantWorkBudget -= importantWorkCharge;
        *allowNormalWork = true;

        if (d_idleWorkBudget >= idleWorkCharge) {
            d_idleWorkBudget -= idleWorkCharge;
            *allowIdleWork = true;
        }
        else {
            *allowIdleWork = false;
        }
    }
    else {
        *allowNormalWork = false;
        *allowIdleWork = false;
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
