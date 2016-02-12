// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "InitModules.h"

#include "bindings/modules/v8/ModuleBindingsInitializer.h"
#include "core/EventTypeNames.h"
#include "core/dom/Document.h"
#include "core/html/HTMLCanvasElement.h"
#include "modules/EventModulesFactory.h"
#include "modules/EventModulesNames.h"
#include "modules/EventTargetModulesNames.h"
#include "modules/IndexedDBNames.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/canvas2d/CanvasRenderingContext2D.h"
#include "modules/filesystem/DraggedIsolatedFileSystemImpl.h"
#include "modules/imagebitmap/ImageBitmapRenderingContext.h"
#include "modules/webdatabase/DatabaseManager.h"
#include "modules/webgl/WebGL2RenderingContext.h"
#include "modules/webgl/WebGLRenderingContext.h"

namespace blink {

void ModulesInitializer::init()
{
    ASSERT(!isInitialized());

    // Strings must be initialized before calling CoreInitializer::init().
    const unsigned modulesStaticStringsCount = EventNames::EventModulesNamesCount
        + EventTargetNames::EventTargetModulesNamesCount
        + IndexedDBNames::IndexedDBNamesCount;
    StringImpl::reserveStaticStringsCapacityForSize(modulesStaticStringsCount);

    EventNames::initModules();
    EventTargetNames::initModules();
    Document::registerEventFactory(EventModulesFactory::create());
    ModuleBindingsInitializer::init();
    IndexedDBNames::init();
    AXObjectCache::init(AXObjectCacheImpl::create);
    DraggedIsolatedFileSystem::init(DraggedIsolatedFileSystemImpl::prepareForDataObject);

    CoreInitializer::init();

    // Canvas context types must be registered with the HTMLCanvasElement.
    HTMLCanvasElement::registerRenderingContextFactory(adoptPtr(new CanvasRenderingContext2D::Factory()));
    HTMLCanvasElement::registerRenderingContextFactory(adoptPtr(new WebGLRenderingContext::Factory()));
    HTMLCanvasElement::registerRenderingContextFactory(adoptPtr(new WebGL2RenderingContext::Factory()));
    HTMLCanvasElement::registerRenderingContextFactory(adoptPtr(new ImageBitmapRenderingContext::Factory()));

    ASSERT(isInitialized());
}

void ModulesInitializer::terminateThreads()
{
    DatabaseManager::terminateDatabaseThread();
}

} // namespace blink
