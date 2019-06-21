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

#include <blpwtk2_timerpumpscheduler.h>

namespace blpwtk2 {

                        // ------------------------
                        // class TimerPumpScheduler
                        // ------------------------

// static
const std::string& TimerPumpScheduler::name()
{
    static std::string schedulerName = "timer";
    return schedulerName;
}

// static
const wchar_t* TimerPumpScheduler::getClassName()
{
    static const wchar_t* className;

    if (nullptr == className) {
        className = L"blpwtk2::TimerPumpScheduler";

        WNDCLASSW windowClass = {
            0,                  // style
            &windowProcedure,   // lpfnWndProc
            0,                  // cbClsExtra
            0,                  // cbWndExtra
            0,                  // hInstance
            0,                  // hIcon
            0,                  // hCursor
            0,                  // hbrBackground
            0,                  // lpszMenuName
            className           // lpszClassName
        };

        ATOM atom = ::RegisterClassW(&windowClass);
        CHECK(atom);
    }

    return className;
}

// static
LRESULT CALLBACK TimerPumpScheduler::windowProcedure(NativeView window_handle,
                                                     UINT message,
                                                     WPARAM wparam,
                                                     LPARAM lparam)
{
    TimerPumpScheduler* scheduler = reinterpret_cast<TimerPumpScheduler*>(wparam);

    if (WM_TIMER == message) {
        scheduler->d_lastTick = GetTickCount();
    }

    return DefWindowProc(window_handle, message, wparam, lparam);
}

TimerPumpScheduler::TimerPumpScheduler()
{
    d_window = ::CreateWindowW(getClassName(),   // lpClassName
                               0,                // lpWindowName
                               0,                // dwStyle
                               CW_DEFAULT,       // x
                               CW_DEFAULT,       // y
                               CW_DEFAULT,       // nWidth
                               CW_DEFAULT,       // nHeight
                               HWND_MESSAGE,     // hwndParent
                               0,                // hMenu
                               0,                // hInstance
                               0);               // lpParam

    d_tunables.push_back({"timer-period", 50});
    d_tunables.push_back({"normal-timeout", 100});
    d_tunables.push_back({"idle-timeout", 75});

    int timerPeriod = d_tunables[0].second;
    SetTimer(d_window, reinterpret_cast<WPARAM>(this), timerPeriod, nullptr);
}

TimerPumpScheduler::~TimerPumpScheduler()
{
    KillTimer(d_window, reinterpret_cast<WPARAM>(this));
    ::DestroyWindow(d_window);
}

void TimerPumpScheduler::isReadyToWork(bool *allowNormalWork, bool *allowIdleWork)
{
    unsigned int normalTimeout = d_tunables[1].second;
    unsigned int idleTimeout = d_tunables[2].second;
    unsigned int currentTickCount = GetTickCount();

    if ((currentTickCount - d_lastTick) < normalTimeout) {
        *allowNormalWork = true;
    }

    if ((currentTickCount - d_lastTick) < idleTimeout) {
        *allowIdleWork = true;
    }
}

std::vector<std::string> TimerPumpScheduler::listTunables()
{
    std::vector<std::string> tunables;

    for (const auto& tunable : d_tunables) {
        tunables.push_back(tunable.first);
    }

    return tunables;
}

int TimerPumpScheduler::setTunable(unsigned index, int value)
{
    if (index < d_tunables.size()) {
        d_tunables[index].second = value;

        if (0 == index) {
            KillTimer(d_window, reinterpret_cast<WPARAM>(this));
            SetTimer(d_window, reinterpret_cast<WPARAM>(this), value, nullptr);
        }

        return 0;
    }

    return -1;
}

}  // close namespace blpwtk2

// vim: ts=4 et
