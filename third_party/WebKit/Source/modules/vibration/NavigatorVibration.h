/*
 *  Copyright (C) 2012 Samsung Electronics
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef NavigatorVibration_h
#define NavigatorVibration_h

#include "core/page/Navigator.h"
#include "core/platform/Timer.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class NavigatorVibration : public Supplement<Navigator> {
public:
    typedef Vector<unsigned> VibrationPattern;

    ~NavigatorVibration();

    bool vibrate(const VibrationPattern&);
    void cancelVibration();
    // FIXME : Hook suspendVibration() and resumeVibration() into the page
    // visibility feature, when the document.hidden attribute is changed.
    void suspendVibration();
    void resumeVibration();
    void timerStartFired(Timer<NavigatorVibration>*);
    void timerStopFired(Timer<NavigatorVibration>*);

    static bool vibrate(Navigator*, unsigned time);
    static bool vibrate(Navigator*, const VibrationPattern&);
    static NavigatorVibration* from(Navigator*);

private:
    NavigatorVibration();
    static const char* supplementName();

    Timer<NavigatorVibration> m_timerStart;
    Timer<NavigatorVibration> m_timerStop;
    bool m_isVibrating;
    VibrationPattern m_pattern;
};

} // namespace WebCore

#endif // NavigatorVibration_h
