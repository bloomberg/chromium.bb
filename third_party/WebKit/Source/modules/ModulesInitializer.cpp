// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ModulesInitializer.h"

#include "bindings/modules/v8/ModuleBindingsInitializer.h"
#include "core/EventTypeNames.h"
#include "core/css/CSSPaintImageGenerator.h"
#include "core/dom/Document.h"
#include "core/editing/suggestion/TextSuggestionBackendImpl.h"
#include "core/exported/WebSharedWorkerImpl.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/inspector/InspectorSession.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "core/origin_trials/OriginTrials.h"
#include "core/page/ChromeClient.h"
#include "core/workers/Worker.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerContentSettingsClient.h"
#include "modules/EventModulesFactory.h"
#include "modules/EventModulesNames.h"
#include "modules/EventTargetModulesNames.h"
#include "modules/IndexedDBNames.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/accessibility/InspectorAccessibilityAgent.h"
#include "modules/app_banner/AppBannerController.h"
#include "modules/audio_output_devices/AudioOutputDeviceClient.h"
#include "modules/audio_output_devices/AudioOutputDeviceClientImpl.h"
#include "modules/audio_output_devices/HTMLMediaElementAudioOutputDevice.h"
#include "modules/cachestorage/InspectorCacheStorageAgent.h"
#include "modules/canvas2d/CanvasRenderingContext2D.h"
#include "modules/compositorworker/CompositorWorkerThread.h"
#include "modules/csspaint/CSSPaintImageGeneratorImpl.h"
#include "modules/device_orientation/DeviceMotionController.h"
#include "modules/device_orientation/DeviceOrientationAbsoluteController.h"
#include "modules/device_orientation/DeviceOrientationController.h"
#include "modules/device_orientation/DeviceOrientationInspectorAgent.h"
#include "modules/document_metadata/CopylessPasteServer.h"
#include "modules/encryptedmedia/HTMLMediaElementEncryptedMedia.h"
#include "modules/exported/WebEmbeddedWorkerImpl.h"
#include "modules/filesystem/DraggedIsolatedFileSystemImpl.h"
#include "modules/filesystem/LocalFileSystemClient.h"
#include "modules/gamepad/NavigatorGamepad.h"
#include "modules/imagebitmap/ImageBitmapRenderingContext.h"
#include "modules/indexeddb/IndexedDBClientImpl.h"
#include "modules/indexeddb/InspectorIndexedDBAgent.h"
#include "modules/installation/InstallationServiceImpl.h"
#include "modules/installedapp/InstalledAppController.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "modules/mediastream/UserMediaClientImpl.h"
#include "modules/mediastream/UserMediaController.h"
#include "modules/navigatorcontentutils/NavigatorContentUtils.h"
#include "modules/navigatorcontentutils/NavigatorContentUtilsClient.h"
#include "modules/offscreencanvas2d/OffscreenCanvasRenderingContext2D.h"
#include "modules/presentation/PresentationController.h"
#include "modules/presentation/PresentationReceiver.h"
#include "modules/push_messaging/PushController.h"
#include "modules/remoteplayback/HTMLMediaElementRemotePlayback.h"
#include "modules/remoteplayback/RemotePlayback.h"
#include "modules/screen_orientation/ScreenOrientationControllerImpl.h"
#include "modules/serviceworkers/NavigatorServiceWorker.h"
#include "modules/serviceworkers/ServiceWorkerLinkResource.h"
#include "modules/storage/DOMWindowStorageController.h"
#include "modules/storage/InspectorDOMStorageAgent.h"
#include "modules/time_zone_monitor/TimeZoneMonitorClient.h"
#include "modules/vr/NavigatorVR.h"
#include "modules/vr/VRController.h"
#include "modules/webdatabase/DatabaseManager.h"
#include "modules/webdatabase/InspectorDatabaseAgent.h"
#include "modules/webgl/WebGL2RenderingContext.h"
#include "modules/webgl/WebGLRenderingContext.h"
#include "platform/mojo/MojoHelper.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/InterfaceRegistry.h"
#include "public/platform/WebSecurityOrigin.h"

namespace blink {

void ModulesInitializer::Initialize() {
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
}

void ModulesInitializer::InitLocalFrame(LocalFrame& frame) const {
  // CoreInitializer::RegisterLocalFrameInitCallback([](LocalFrame& frame) {
  if (frame.IsMainFrame()) {
    frame.GetInterfaceRegistry()->AddInterface(WTF::Bind(
        &CopylessPasteServer::BindMojoRequest, WrapWeakPersistent(&frame)));
  }
  frame.GetInterfaceRegistry()->AddInterface(
      WTF::Bind(&InstallationServiceImpl::Create, WrapWeakPersistent(&frame)));
  // TODO(dominickn): This interface should be document-scoped rather than
  // frame-scoped, as the resulting banner event is dispatched to
  // frame()->document().
  frame.GetInterfaceRegistry()->AddInterface(WTF::Bind(
      &AppBannerController::BindMojoRequest, WrapWeakPersistent(&frame)));
  frame.GetInterfaceRegistry()->AddInterface(WTF::Bind(
      &TextSuggestionBackendImpl::Create, WrapWeakPersistent(&frame)));
}

void ModulesInitializer::InstallSupplements(LocalFrame& frame) const {
  WebLocalFrameBase* web_frame = WebLocalFrameBase::FromFrame(&frame);
  WebFrameClient* client = web_frame->Client();
  DCHECK(client);
  ProvidePushControllerTo(frame, client->PushClient());
  ProvideUserMediaTo(frame,
                     UserMediaClientImpl::Create(client->UserMediaClient()));
  ProvideIndexedDBClientTo(frame, IndexedDBClientImpl::Create(frame));
  ProvideLocalFileSystemTo(frame, LocalFileSystemClient::Create());
  NavigatorContentUtils::ProvideTo(
      *frame.DomWindow()->navigator(),
      NavigatorContentUtilsClient::Create(web_frame));

  ScreenOrientationControllerImpl::ProvideTo(
      frame, client->GetWebScreenOrientationClient());
  if (RuntimeEnabledFeatures::PresentationEnabled())
    PresentationController::ProvideTo(frame, client->PresentationClient());
  if (RuntimeEnabledFeatures::AudioOutputDevicesEnabled()) {
    ProvideAudioOutputDeviceClientTo(frame,
                                     new AudioOutputDeviceClientImpl(frame));
  }
  InstalledAppController::ProvideTo(frame, client->GetRelatedAppsFetcher());
}

void ModulesInitializer::ProvideLocalFileSystemToWorker(
    WorkerClients& worker_clients) const {
  ::blink::ProvideLocalFileSystemToWorker(&worker_clients,
                                          LocalFileSystemClient::Create());
}

void ModulesInitializer::ProvideIndexedDBClientToWorker(
    WorkerClients& worker_clients) const {
  ::blink::ProvideIndexedDBClientToWorker(
      &worker_clients, IndexedDBClientImpl::Create(worker_clients));
}

MediaControls* ModulesInitializer::CreateMediaControls(
    HTMLMediaElement& media_element,
    ShadowRoot& shadow_root) const {
  return MediaControlsImpl::Create(media_element, shadow_root);
}

void ModulesInitializer::InitInspectorAgentSession(
    InspectorSession* session,
    bool allow_view_agents,
    InspectorDOMAgent* dom_agent,
    InspectedFrames* inspected_frames,
    Page* page) const {
  session->Append(
      new InspectorIndexedDBAgent(inspected_frames, session->V8Session()));
  session->Append(new DeviceOrientationInspectorAgent(inspected_frames));
  if (allow_view_agents) {
    session->Append(InspectorDatabaseAgent::Create(page));
    session->Append(new InspectorAccessibilityAgent(page, dom_agent));
    session->Append(InspectorDOMStorageAgent::Create(page));
    session->Append(InspectorCacheStorageAgent::Create(inspected_frames));
  }
}

LinkResource* ModulesInitializer::CreateServiceWorkerLinkResource(
    HTMLLinkElement* owner) const {
  return ServiceWorkerLinkResource::Create(owner);
}

void ModulesInitializer::OnClearWindowObjectInMainWorld(
    Document& document,
    const Settings& settings) {
  DeviceMotionController::From(document);
  DeviceOrientationController::From(document);
  DeviceOrientationAbsoluteController::From(document);
  NavigatorGamepad::From(document);
  NavigatorServiceWorker::From(document);
  DOMWindowStorageController::From(document);
  if (RuntimeEnabledFeatures::WebVREnabled() ||
      OriginTrials::webVREnabled(document.GetExecutionContext()))
    NavigatorVR::From(document);
  if (RuntimeEnabledFeatures::PresentationEnabled() &&
      settings.GetPresentationReceiver()) {
    // Call this in order to ensure the object is created.
    PresentationReceiver::From(document);
  }
}

std::unique_ptr<WebMediaPlayer> ModulesInitializer::CreateWebMediaPlayer(
    WebFrameClient* web_frame_client,
    HTMLMediaElement& html_media_element,
    const WebMediaPlayerSource& source,
    WebMediaPlayerClient* media_player_client) {
  HTMLMediaElementEncryptedMedia& encrypted_media =
      HTMLMediaElementEncryptedMedia::From(html_media_element);
  WebString sink_id(
      HTMLMediaElementAudioOutputDevice::sinkId(html_media_element));
  return WTF::WrapUnique(web_frame_client->CreateMediaPlayer(
      source, media_player_client, &encrypted_media,
      encrypted_media.ContentDecryptionModule(), sink_id));
}

WebRemotePlaybackClient* ModulesInitializer::CreateWebRemotePlaybackClient(
    HTMLMediaElement& html_media_element) {
  return HTMLMediaElementRemotePlayback::remote(html_media_element);
}

}  // namespace blink
