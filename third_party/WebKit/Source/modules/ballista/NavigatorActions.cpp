// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/ballista/NavigatorActions.h"

#include "config.h"
#include "core/frame/Navigator.h"
#include "modules/ballista/Actions.h"

namespace blink {

NavigatorActions& NavigatorActions::from(Navigator& navigator)
{
    NavigatorActions* supplement = static_cast<NavigatorActions*>(Supplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        supplement = new NavigatorActions();
        provideTo(navigator, supplementName(), supplement);
    }
    return *supplement;
}

Actions* NavigatorActions::actions(Navigator& navigator)
{
    return NavigatorActions::from(navigator).actions();
}

Actions* NavigatorActions::actions()
{
    if (!m_actions)
        m_actions = Actions::create();
    return m_actions.get();
}

DEFINE_TRACE(NavigatorActions)
{
    visitor->trace(m_actions);
    Supplement<Navigator>::trace(visitor);
}

NavigatorActions::NavigatorActions()
{
}

const char* NavigatorActions::supplementName()
{
    return "NavigatorActions";
}

} // namespace blink
