// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ModulesInitializer.h"

#include "bindings/modules/v8/ModuleBindingsInitializer.h"
#include "core/EventTypeNames.h"
#include "core/css/CSSPaintImageGenerator.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "modules/EventModulesFactory.h"
#include "modules/EventModulesNames.h"
#include "modules/EventTargetModulesNames.h"
#include "modules/IndexedDBNames.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/app_banner/AppBannerController.h"
#include "modules/canvas2d/CanvasRenderingContext2D.h"
#include "modules/compositorworker/CompositorWorkerThread.h"
#include "modules/csspaint/CSSPaintImageGeneratorImpl.h"
#include "modules/document_metadata/CopylessPasteServer.h"
#include "modules/filesystem/DraggedIsolatedFileSystemImpl.h"
#include "modules/imagebitmap/ImageBitmapRenderingContext.h"
#include "modules/installation/InstallationServiceImpl.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "modules/offscreencanvas2d/OffscreenCanvasRenderingContext2D.h"
#include "modules/time_zone_monitor/TimeZoneMonitorClient.h"
#include "modules/webdatabase/DatabaseManager.h"
#include "modules/webgl/WebGL2RenderingContext.h"
#include "modules/webgl/WebGLRenderingContext.h"
#include "platform/mojo/MojoHelper.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/InterfaceRegistry.h"

namespace blink {

void ModulesInitializer::Initialize() {
  DCHECK(!IsInitialized());

  // Strings must be initialized before calling CoreInitializer::init().
  const unsigned kModulesStaticStringsCount =
      EventNames::EventModulesNamesCount +
      EventTargetNames::EventTargetModulesNamesCount +
      IndexedDBNames::IndexedDBNamesCount;
  StringImpl::ReserveStaticStringsCapacityForSize(kModulesStaticStringsCount);

  EventNames::initModules();
  EventTargetNames::initModules();
  Document::RegisterEventFactory(EventModulesFactory::Create());
  ModuleBindingsInitializer::Init();
  IndexedDBNames::init();
  AXObjectCache::Init(AXObjectCacheImpl::Create);
  DraggedIsolatedFileSystem::Init(
      DraggedIsolatedFileSystemImpl::PrepareForDataObject);
  CSSPaintImageGenerator::Init(CSSPaintImageGeneratorImpl::Create);
  // Some unit tests may have no message loop ready, so we can't initialize the
  // mojo stuff here. They can initialize those mojo stuff they're interested in
  // later after they got a message loop ready.
  if (CanInitializeMojo())
    TimeZoneMonitorClient::Init();

  CoreInitializer::Initialize();

  // Canvas context types must be registered with the HTMLCanvasElement.
  HTMLCanvasElement::RegisterRenderingContextFactory(
      WTF::MakeUnique<CanvasRenderingContext2D::Factory>());
  HTMLCanvasElement::RegisterRenderingContextFactory(
      WTF::MakeUnique<WebGLRenderingContext::Factory>());
  HTMLCanvasElement::RegisterRenderingContextFactory(
      WTF::MakeUnique<WebGL2RenderingContext::Factory>());
  HTMLCanvasElement::RegisterRenderingContextFactory(
      WTF::MakeUnique<ImageBitmapRenderingContext::Factory>());

  // OffscreenCanvas context types must be registered with the OffscreenCanvas.
  OffscreenCanvas::RegisterRenderingContextFactory(
      WTF::MakeUnique<OffscreenCanvasRenderingContext2D::Factory>());
  OffscreenCanvas::RegisterRenderingContextFactory(
      WTF::MakeUnique<WebGLRenderingContext::Factory>());
  OffscreenCanvas::RegisterRenderingContextFactory(
      WTF::MakeUnique<WebGL2RenderingContext::Factory>());

  // Mojo Interfaces registered with LocalFrame
  LocalFrame::RegisterInitializationCallback([](LocalFrame* frame) {
    if (frame && frame->IsMainFrame()) {
      frame->GetInterfaceRegistry()->AddInterface(WTF::Bind(
          &CopylessPasteServer::BindMojoRequest, WrapWeakPersistent(frame)));
    }
    frame->GetInterfaceRegistry()->AddInterface(
        WTF::Bind(&InstallationServiceImpl::Create, WrapWeakPersistent(frame)));
    // TODO(dominickn): This interface should be document-scoped rather than
    // frame-scoped, as the resulting banner event is dispatched to
    // frame()->document().
    frame->GetInterfaceRegistry()->AddInterface(WTF::Bind(
        &AppBannerController::BindMojoRequest, WrapWeakPersistent(frame)));
  });

  HTMLMediaElement::RegisterMediaControlsFactory(
      WTF::MakeUnique<MediaControlsImpl::Factory>());

  DCHECK(IsInitialized());
}

}  // namespace blink
