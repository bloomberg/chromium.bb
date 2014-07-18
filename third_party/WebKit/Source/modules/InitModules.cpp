// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "InitModules.h"

#include "bindings/modules/v8/ModuleBindingsInitializer.h"
#include "core/EventTypeNames.h"
#include "core/dom/Document.h"
#include "modules/EventModulesFactory.h"
#include "modules/EventModulesNames.h"
#include "modules/EventTargetModulesNames.h"

namespace blink {

void ModulesInitializer::initEventNames()
{
    EventNames::init();
    EventNames::initModules();
}

void ModulesInitializer::initEventTargetNames()
{
    EventTargetNames::init();
    EventTargetNames::initModules();
}

void ModulesInitializer::registerEventFactory()
{
    CoreInitializer::registerEventFactory();
    Document::registerEventFactory(EventModulesFactory::create());
}

void ModulesInitializer::initBindings()
{
    ModuleBindingsInitializer::init();
}

} // namespace blink
