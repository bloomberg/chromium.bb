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
#include "modules/IndexedDBNames.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/filesystem/DraggedIsolatedFileSystemImpl.h"
#include "modules/webdatabase/DatabaseManager.h"

namespace blink {

void ModulesInitializer::init()
{
    ASSERT(!isInitialized());

    // Strings must be initialized before calling CoreInitializer::init().
    EventNames::initModules();
    EventTargetNames::initModules();
    Document::registerEventFactory(EventModulesFactory::create());
    ModuleBindingsInitializer::init();
    IndexedDBNames::init();
    AXObjectCache::init(AXObjectCacheImpl::create);
    DraggedIsolatedFileSystem::init(DraggedIsolatedFileSystemImpl::prepareForDataObject);

    CoreInitializer::init();

    ASSERT(isInitialized());
}

void ModulesInitializer::terminateThreads()
{
    DatabaseManager::terminateDatabaseThread();
}

} // namespace blink
