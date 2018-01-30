// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/ModulesInitializer.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "bindings/modules/v8/ModuleBindingsInitializer.h"
#include "core/css/CSSPaintImageGenerator.h"
#include "core/dom/ContextFeaturesClientImpl.h"
#include "core/dom/Document.h"
#include "core/editing/suggestion/TextSuggestionBackendImpl.h"
#include "core/event_type_names.h"
#include "core/exported/WebSharedWorkerImpl.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/html/canvas/HTMLCanvasElement.h"
#include "core/html/media/HTMLMediaElement.h"
#include "core/inspector/InspectorSession.h"
#include "core/leak_detector/BlinkLeakDetector.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "core/origin_trials/origin_trials.h"
#include "core/page/ChromeClient.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerContentSettingsClient.h"
#include "modules/EventModulesFactory.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/accessibility/InspectorAccessibilityAgent.h"
#include "modules/animationworklet/AnimationWorkletThread.h"
#include "modules/app_banner/AppBannerController.h"
#include "modules/audio_output_devices/AudioOutputDeviceClient.h"
#include "modules/audio_output_devices/AudioOutputDeviceClientImpl.h"
#include "modules/audio_output_devices/HTMLMediaElementAudioOutputDevice.h"
#include "modules/cachestorage/InspectorCacheStorageAgent.h"
#include "modules/canvas/canvas2d/CanvasRenderingContext2D.h"
#include "modules/canvas/imagebitmap/ImageBitmapRenderingContext.h"
#include "modules/canvas/offscreencanvas2d/OffscreenCanvasRenderingContext2D.h"
#include "modules/csspaint/CSSPaintImageGeneratorImpl.h"
#include "modules/device_orientation/DeviceMotionController.h"
#include "modules/device_orientation/DeviceOrientationAbsoluteController.h"
#include "modules/device_orientation/DeviceOrientationController.h"
#include "modules/device_orientation/DeviceOrientationInspectorAgent.h"
#include "modules/document_metadata/CopylessPasteServer.h"
#include "modules/encryptedmedia/HTMLMediaElementEncryptedMedia.h"
#include "modules/encryptedmedia/MediaKeysController.h"
#include "modules/event_modules_names.h"
#include "modules/event_target_modules_names.h"
#include "modules/exported/WebEmbeddedWorkerImpl.h"
#include "modules/filesystem/DraggedIsolatedFileSystemImpl.h"
#include "modules/filesystem/LocalFileSystemClient.h"
#include "modules/gamepad/NavigatorGamepad.h"
#include "modules/indexed_db_names.h"
#include "modules/indexeddb/IndexedDBClient.h"
#include "modules/indexeddb/InspectorIndexedDBAgent.h"
#include "modules/installation/InstallationServiceImpl.h"
#include "modules/installedapp/InstalledAppController.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "modules/mediastream/UserMediaClient.h"
#include "modules/mediastream/UserMediaController.h"
#include "modules/navigatorcontentutils/NavigatorContentUtils.h"
#include "modules/navigatorcontentutils/NavigatorContentUtilsClient.h"
#include "modules/presentation/PresentationController.h"
#include "modules/presentation/PresentationReceiver.h"
#include "modules/push_messaging/PushController.h"
#include "modules/remoteplayback/HTMLMediaElementRemotePlayback.h"
#include "modules/remoteplayback/RemotePlayback.h"
#include "modules/screen_orientation/ScreenOrientationControllerImpl.h"
#include "modules/serviceworkers/NavigatorServiceWorker.h"
#include "modules/speech/SpeechRecognitionClientProxy.h"
#include "modules/storage/DOMWindowStorageController.h"
#include "modules/storage/InspectorDOMStorageAgent.h"
#include "modules/storage/StorageNamespaceController.h"
#include "modules/time_zone_monitor/TimeZoneMonitorClient.h"
#include "modules/vr/NavigatorVR.h"
#include "modules/vr/VRController.h"
#include "modules/webdatabase/DatabaseClient.h"
#include "modules/webdatabase/DatabaseManager.h"
#include "modules/webdatabase/InspectorDatabaseAgent.h"
#include "modules/webdatabase/WebDatabaseImpl.h"
#include "modules/webgl/WebGL2RenderingContext.h"
#include "modules/webgl/WebGLRenderingContext.h"
#include "modules/xr/XRPresentationContext.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/mojo/MojoHelper.h"
#include "platform/wtf/Functional.h"
#include "public/platform/InterfaceRegistry.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/web/WebViewClient.h"

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
  if (CanInitializeMojo()) {
    TimeZoneMonitorClient::Init();
  }

  CoreInitializer::Initialize();

  // Canvas context types must be registered with the HTMLCanvasElement.
  HTMLCanvasElement::RegisterRenderingContextFactory(
      std::make_unique<CanvasRenderingContext2D::Factory>());
  HTMLCanvasElement::RegisterRenderingContextFactory(
      std::make_unique<WebGLRenderingContext::Factory>());
  HTMLCanvasElement::RegisterRenderingContextFactory(
      std::make_unique<WebGL2RenderingContext::Factory>());
  HTMLCanvasElement::RegisterRenderingContextFactory(
      std::make_unique<ImageBitmapRenderingContext::Factory>());
  HTMLCanvasElement::RegisterRenderingContextFactory(
      std::make_unique<XRPresentationContext::Factory>());

  // OffscreenCanvas context types must be registered with the OffscreenCanvas.
  OffscreenCanvas::RegisterRenderingContextFactory(
      std::make_unique<OffscreenCanvasRenderingContext2D::Factory>());
  OffscreenCanvas::RegisterRenderingContextFactory(
      std::make_unique<WebGLRenderingContext::Factory>());
  OffscreenCanvas::RegisterRenderingContextFactory(
      std::make_unique<WebGL2RenderingContext::Factory>());
}

void ModulesInitializer::InitLocalFrame(LocalFrame& frame) const {
  // CoreInitializer::RegisterLocalFrameInitCallback([](LocalFrame& frame) {
  if (frame.IsMainFrame()) {
    frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
        &CopylessPasteServer::BindMojoRequest, WrapWeakPersistent(&frame)));
  }
  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &InstallationServiceImpl::Create, WrapWeakPersistent(&frame)));
  // TODO(dominickn): This interface should be document-scoped rather than
  // frame-scoped, as the resulting banner event is dispatched to
  // frame()->document().
  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &AppBannerController::BindMojoRequest, WrapWeakPersistent(&frame)));
  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &TextSuggestionBackendImpl::Create, WrapWeakPersistent(&frame)));
}

void ModulesInitializer::InstallSupplements(LocalFrame& frame) const {
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(&frame);
  WebFrameClient* client = web_frame->Client();
  DCHECK(client);
  ProvidePushControllerTo(frame, client->PushClient());
  ProvideUserMediaTo(frame, UserMediaClient::Create(client->UserMediaClient()));
  ProvideIndexedDBClientTo(frame, IndexedDBClient::Create(frame));
  ProvideLocalFileSystemTo(frame, LocalFileSystemClient::Create());
  NavigatorContentUtils::ProvideTo(
      *frame.DomWindow()->navigator(),
      NavigatorContentUtilsClient::Create(web_frame));

  ScreenOrientationControllerImpl::ProvideTo(frame);
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
      &worker_clients, IndexedDBClient::Create(worker_clients));
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

void ModulesInitializer::OnClearWindowObjectInMainWorld(
    Document& document,
    const Settings& settings) const {
  DeviceMotionController::From(document);
  DeviceOrientationController::From(document);
  DeviceOrientationAbsoluteController::From(document);
  NavigatorGamepad::From(document);
  NavigatorServiceWorker::From(document);
  DOMWindowStorageController::From(document);
  if (OriginTrials::webVREnabled(document.GetExecutionContext()))
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
    WebMediaPlayerClient* media_player_client,
    WebLayerTreeView* view) const {
  HTMLMediaElementEncryptedMedia& encrypted_media =
      HTMLMediaElementEncryptedMedia::From(html_media_element);
  WebString sink_id(
      HTMLMediaElementAudioOutputDevice::sinkId(html_media_element));
  return base::WrapUnique(web_frame_client->CreateMediaPlayer(
      source, media_player_client, &encrypted_media,
      encrypted_media.ContentDecryptionModule(), sink_id, view));
}

WebRemotePlaybackClient* ModulesInitializer::CreateWebRemotePlaybackClient(
    HTMLMediaElement& html_media_element) const {
  return HTMLMediaElementRemotePlayback::remote(html_media_element);
}

void ModulesInitializer::ProvideModulesToPage(Page& page,
                                              WebViewClient* client) const {
  MediaKeysController::ProvideMediaKeysTo(page);
  ::blink::ProvideContextFeaturesTo(page, ContextFeaturesClientImpl::Create());
  ::blink::ProvideDatabaseClientTo(page, new DatabaseClient);
  StorageNamespaceController::ProvideStorageNamespaceTo(page, client);
  ::blink::ProvideSpeechRecognitionTo(
      page, SpeechRecognitionClientProxy::Create(
                client ? client->SpeechRecognizer() : nullptr));
}

void ModulesInitializer::ForceNextWebGLContextCreationToFail() const {
  WebGLRenderingContext::ForceNextWebGLContextCreationToFail();
}

void ModulesInitializer::CollectAllGarbageForAnimationWorklet() const {
  AnimationWorkletThread::CollectAllGarbage();
}

void ModulesInitializer::RegisterInterfaces(
    service_manager::BinderRegistry& registry) {
  DCHECK(Platform::Current());
  registry.AddInterface(
      ConvertToBaseCallback(blink::CrossThreadBind(&WebDatabaseImpl::Create)),
      Platform::Current()->GetIOTaskRunner());
}

}  // namespace blink
