// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/imagecapture/ImageCapture.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/fileapi/Blob.h"
#include "core/frame/ImageBitmap.h"
#include "modules/EventTargetModules.h"
#include "modules/imagecapture/MediaSettingsRange.h"
#include "modules/imagecapture/PhotoCapabilities.h"
#include "modules/imagecapture/PhotoSettings.h"
#include "modules/mediastream/MediaStreamTrack.h"
#include "modules/mediastream/MediaTrackCapabilities.h"
#include "platform/WaitableEvent.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/WebImageCaptureFrameGrabber.h"
#include "public/platform/WebMediaStreamTrack.h"
#include "wtf/PtrUtil.h"

namespace blink {

namespace {

const char kNoServiceError[] = "ImageCapture service unavailable.";

bool trackIsInactive(const MediaStreamTrack& track) {
  // Spec instructs to return an exception if the Track's readyState() is not
  // "live". Also reject if the track is disabled or muted.
  return track.readyState() != "live" || !track.enabled() || track.muted();
}

media::mojom::blink::MeteringMode parseMeteringMode(const String& blinkMode) {
  if (blinkMode == "manual")
    return media::mojom::blink::MeteringMode::MANUAL;
  if (blinkMode == "single-shot")
    return media::mojom::blink::MeteringMode::SINGLE_SHOT;
  if (blinkMode == "continuous")
    return media::mojom::blink::MeteringMode::CONTINUOUS;
  return media::mojom::blink::MeteringMode::NONE;
}

media::mojom::blink::FillLightMode parseFillLightMode(const String& blinkMode) {
  if (blinkMode == "off")
    return media::mojom::blink::FillLightMode::OFF;
  if (blinkMode == "auto")
    return media::mojom::blink::FillLightMode::AUTO;
  if (blinkMode == "flash")
    return media::mojom::blink::FillLightMode::FLASH;
  if (blinkMode == "torch")
    return media::mojom::blink::FillLightMode::TORCH;
  return media::mojom::blink::FillLightMode::NONE;
}

WebString toString(media::mojom::blink::MeteringMode value) {
  switch (value) {
    case media::mojom::blink::MeteringMode::NONE:
      return WebString::fromUTF8("none");
    case media::mojom::blink::MeteringMode::MANUAL:
      return WebString::fromUTF8("manual");
    case media::mojom::blink::MeteringMode::SINGLE_SHOT:
      return WebString::fromUTF8("single-shot");
    case media::mojom::blink::MeteringMode::CONTINUOUS:
      return WebString::fromUTF8("continuous");
    default:
      NOTREACHED() << "Unknown MeteringMode";
  }
  return WebString();
}

}  // anonymous namespace

ImageCapture* ImageCapture::create(ExecutionContext* context,
                                   MediaStreamTrack* track,
                                   ExceptionState& exceptionState) {
  if (track->kind() != "video") {
    exceptionState.throwDOMException(
        NotSupportedError,
        "Cannot create an ImageCapturer from a non-video Track.");
    return nullptr;
  }

  return new ImageCapture(context, track);
}

ImageCapture::~ImageCapture() {
  DCHECK(!hasEventListeners());
  // There should be no more outstanding |m_serviceRequests| at this point
  // since each of them holds a persistent handle to this object.
  DCHECK(m_serviceRequests.isEmpty());
}

const AtomicString& ImageCapture::interfaceName() const {
  return EventTargetNames::ImageCapture;
}

ExecutionContext* ImageCapture::getExecutionContext() const {
  return ContextLifecycleObserver::getExecutionContext();
}

bool ImageCapture::hasPendingActivity() const {
  return getExecutionContext() && hasEventListeners();
}

void ImageCapture::contextDestroyed(ExecutionContext*) {
  removeAllEventListeners();
  m_serviceRequests.clear();
  DCHECK(!hasEventListeners());
}

ScriptPromise ImageCapture::getPhotoCapabilities(
    ScriptState* scriptState,
    ExceptionState& exceptionState) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  if (!m_service) {
    resolver->reject(DOMException::create(NotFoundError, kNoServiceError));
    return promise;
  }

  m_serviceRequests.insert(resolver);

  // m_streamTrack->component()->source()->id() is the renderer "name" of the
  // camera;
  // TODO(mcasas) consider sending the security origin as well:
  // scriptState->getExecutionContext()->getSecurityOrigin()->toString()
  m_service->GetCapabilities(
      m_streamTrack->component()->source()->id(),
      convertToBaseCallback(WTF::bind(&ImageCapture::onCapabilities,
                                      wrapPersistent(this),
                                      wrapPersistent(resolver))));
  return promise;
}

ScriptPromise ImageCapture::setOptions(ScriptState* scriptState,
                                       const PhotoSettings& photoSettings,
                                       ExceptionState& exceptionState) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  if (trackIsInactive(*m_streamTrack)) {
    resolver->reject(DOMException::create(
        InvalidStateError, "The associated Track is in an invalid state."));
    return promise;
  }

  if (!m_service) {
    resolver->reject(DOMException::create(NotFoundError, kNoServiceError));
    return promise;
  }

  m_serviceRequests.insert(resolver);

  // TODO(mcasas): should be using a mojo::StructTraits instead.
  media::mojom::blink::PhotoSettingsPtr settings =
      media::mojom::blink::PhotoSettings::New();
  settings->has_zoom = photoSettings.hasZoom();
  if (settings->has_zoom)
    settings->zoom = photoSettings.zoom();
  settings->has_height = photoSettings.hasImageHeight();
  if (settings->has_height)
    settings->height = photoSettings.imageHeight();
  settings->has_width = photoSettings.hasImageWidth();
  if (settings->has_width)
    settings->width = photoSettings.imageWidth();
  settings->has_focus_mode = photoSettings.hasFocusMode();
  if (settings->has_focus_mode)
    settings->focus_mode = parseMeteringMode(photoSettings.focusMode());
  settings->has_exposure_mode = photoSettings.hasExposureMode();
  if (settings->has_exposure_mode)
    settings->exposure_mode = parseMeteringMode(photoSettings.exposureMode());
  settings->has_exposure_compensation = photoSettings.hasExposureCompensation();
  if (settings->has_exposure_compensation)
    settings->exposure_compensation = photoSettings.exposureCompensation();
  settings->has_white_balance_mode = photoSettings.hasWhiteBalanceMode();
  if (settings->has_white_balance_mode)
    settings->white_balance_mode =
        parseMeteringMode(photoSettings.whiteBalanceMode());
  settings->has_iso = photoSettings.hasIso();
  if (settings->has_iso)
    settings->iso = photoSettings.iso();
  settings->has_red_eye_reduction = photoSettings.hasRedEyeReduction();
  if (settings->has_red_eye_reduction)
    settings->red_eye_reduction = photoSettings.redEyeReduction();
  settings->has_fill_light_mode = photoSettings.hasFillLightMode();
  if (settings->has_fill_light_mode)
    settings->fill_light_mode =
        parseFillLightMode(photoSettings.fillLightMode());
  if (photoSettings.hasPointsOfInterest()) {
    for (const auto& point : photoSettings.pointsOfInterest()) {
      auto mojoPoint = media::mojom::blink::Point2D::New();
      mojoPoint->x = point.x();
      mojoPoint->y = point.y();
      settings->points_of_interest.push_back(std::move(mojoPoint));
    }
  }
  settings->has_color_temperature = photoSettings.hasColorTemperature();
  if (settings->has_color_temperature)
    settings->color_temperature = photoSettings.colorTemperature();
  settings->has_brightness = photoSettings.hasBrightness();
  if (settings->has_brightness)
    settings->brightness = photoSettings.brightness();
  settings->has_contrast = photoSettings.hasContrast();
  if (settings->has_contrast)
    settings->contrast = photoSettings.contrast();
  settings->has_saturation = photoSettings.hasSaturation();
  if (settings->has_saturation)
    settings->saturation = photoSettings.saturation();
  settings->has_sharpness = photoSettings.hasSharpness();
  if (settings->has_sharpness)
    settings->sharpness = photoSettings.sharpness();

  m_service->SetOptions(m_streamTrack->component()->source()->id(),
                        std::move(settings),
                        convertToBaseCallback(WTF::bind(
                            &ImageCapture::onSetOptions, wrapPersistent(this),
                            wrapPersistent(resolver))));
  return promise;
}

ScriptPromise ImageCapture::takePhoto(ScriptState* scriptState,
                                      ExceptionState& exceptionState) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  if (trackIsInactive(*m_streamTrack)) {
    resolver->reject(DOMException::create(
        InvalidStateError, "The associated Track is in an invalid state."));
    return promise;
  }

  if (!m_service) {
    resolver->reject(DOMException::create(NotFoundError, kNoServiceError));
    return promise;
  }

  m_serviceRequests.insert(resolver);

  // m_streamTrack->component()->source()->id() is the renderer "name" of the
  // camera;
  // TODO(mcasas) consider sending the security origin as well:
  // scriptState->getExecutionContext()->getSecurityOrigin()->toString()
  m_service->TakePhoto(m_streamTrack->component()->source()->id(),
                       convertToBaseCallback(WTF::bind(
                           &ImageCapture::onTakePhoto, wrapPersistent(this),
                           wrapPersistent(resolver))));
  return promise;
}

ScriptPromise ImageCapture::grabFrame(ScriptState* scriptState,
                                      ExceptionState& exceptionState) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  if (trackIsInactive(*m_streamTrack)) {
    resolver->reject(DOMException::create(
        InvalidStateError, "The associated Track is in an invalid state."));
    return promise;
  }

  // Create |m_frameGrabber| the first time.
  if (!m_frameGrabber) {
    m_frameGrabber =
        WTF::wrapUnique(Platform::current()->createImageCaptureFrameGrabber());
  }

  if (!m_frameGrabber) {
    resolver->reject(DOMException::create(
        UnknownError, "Couldn't create platform resources"));
    return promise;
  }

  // The platform does not know about MediaStreamTrack, so we wrap it up.
  WebMediaStreamTrack track(m_streamTrack->component());
  m_frameGrabber->grabFrame(
      &track, new CallbackPromiseAdapter<ImageBitmap, void>(resolver));

  return promise;
}

MediaTrackCapabilities& ImageCapture::getMediaTrackCapabilities() {
  return m_capabilities;
}

ImageCapture::ImageCapture(ExecutionContext* context, MediaStreamTrack* track)
    : ContextLifecycleObserver(context), m_streamTrack(track) {
  DCHECK(m_streamTrack);
  DCHECK(!m_service.is_bound());

  Platform::current()->interfaceProvider()->getInterface(
      mojo::MakeRequest(&m_service));

  m_service.set_connection_error_handler(convertToBaseCallback(WTF::bind(
      &ImageCapture::onServiceConnectionError, wrapWeakPersistent(this))));

  // Launch a retrieval of the current capabilities, which arrive asynchronously
  // to avoid blocking the main UI thread.
  m_service->GetCapabilities(
      m_streamTrack->component()->source()->id(),
      convertToBaseCallback(WTF::bind(&ImageCapture::onCapabilitiesBootstrap,
                                      wrapPersistent(this))));
}

void ImageCapture::onCapabilities(
    ScriptPromiseResolver* resolver,
    media::mojom::blink::PhotoCapabilitiesPtr capabilities) {
  DVLOG(1) << __func__;
  if (!m_serviceRequests.contains(resolver))
    return;
  if (capabilities.is_null()) {
    resolver->reject(DOMException::create(UnknownError, "platform error"));
  } else {
    PhotoCapabilities* caps = PhotoCapabilities::create();
    // TODO(mcasas): Remove the explicit MediaSettingsRange::create() when
    // mojo::StructTraits supports garbage-collected mappings,
    // https://crbug.com/700180.
    caps->setIso(MediaSettingsRange::create(std::move(capabilities->iso)));
    caps->setImageHeight(
        MediaSettingsRange::create(std::move(capabilities->height)));
    caps->setImageWidth(
        MediaSettingsRange::create(std::move(capabilities->width)));
    caps->setZoom(MediaSettingsRange::create(std::move(capabilities->zoom)));
    caps->setExposureCompensation(MediaSettingsRange::create(
        std::move(capabilities->exposure_compensation)));
    caps->setColorTemperature(
        MediaSettingsRange::create(std::move(capabilities->color_temperature)));
    caps->setBrightness(
        MediaSettingsRange::create(std::move(capabilities->brightness)));
    caps->setContrast(
        MediaSettingsRange::create(std::move(capabilities->contrast)));
    caps->setSaturation(
        MediaSettingsRange::create(std::move(capabilities->saturation)));
    caps->setSharpness(
        MediaSettingsRange::create(std::move(capabilities->sharpness)));

    caps->setFocusMode(capabilities->focus_mode);
    caps->setExposureMode(capabilities->exposure_mode);
    caps->setWhiteBalanceMode(capabilities->white_balance_mode);
    caps->setFillLightMode(capabilities->fill_light_mode);

    caps->setRedEyeReduction(capabilities->red_eye_reduction);
    resolver->resolve(caps);
  }
  m_serviceRequests.erase(resolver);
}

void ImageCapture::onSetOptions(ScriptPromiseResolver* resolver, bool result) {
  if (!m_serviceRequests.contains(resolver))
    return;

  if (result)
    resolver->resolve();
  else
    resolver->reject(DOMException::create(UnknownError, "setOptions failed"));
  m_serviceRequests.erase(resolver);
}

void ImageCapture::onTakePhoto(ScriptPromiseResolver* resolver,
                               media::mojom::blink::BlobPtr blob) {
  if (!m_serviceRequests.contains(resolver))
    return;

  // TODO(mcasas): Should be using a mojo::StructTraits.
  if (blob->data.isEmpty())
    resolver->reject(DOMException::create(UnknownError, "platform error"));
  else
    resolver->resolve(
        Blob::create(blob->data.data(), blob->data.size(), blob->mime_type));
  m_serviceRequests.erase(resolver);
}

void ImageCapture::onCapabilitiesBootstrap(
    media::mojom::blink::PhotoCapabilitiesPtr capabilities) {
  DVLOG(1) << __func__;
  if (capabilities.is_null())
    return;

  // TODO(mcasas): adapt the mojo interface to return a list of supported Modes
  // when moving these out of PhotoCapabilities, https://crbug.com/700607.
  m_capabilities.setWhiteBalanceMode(
      WTF::Vector<WTF::String>({toString(capabilities->white_balance_mode)}));
  m_capabilities.setExposureMode(
      WTF::Vector<WTF::String>({toString(capabilities->exposure_mode)}));
  m_capabilities.setFocusMode(
      WTF::Vector<WTF::String>({toString(capabilities->focus_mode)}));

  // TODO(mcasas): Remove the explicit MediaSettingsRange::create() when
  // mojo::StructTraits supports garbage-collected mappings,
  // https://crbug.com/700180.
  if (capabilities->exposure_compensation->max !=
      capabilities->exposure_compensation->min) {
    m_capabilities.setExposureCompensation(MediaSettingsRange::create(
        std::move(capabilities->exposure_compensation)));
  }
  if (capabilities->color_temperature->max !=
      capabilities->color_temperature->min) {
    m_capabilities.setColorTemperature(
        MediaSettingsRange::create(std::move(capabilities->color_temperature)));
  }
  if (capabilities->iso->max != capabilities->iso->min) {
    m_capabilities.setIso(
        MediaSettingsRange::create(std::move(capabilities->iso)));
  }

  if (capabilities->brightness->max != capabilities->brightness->min) {
    m_capabilities.setBrightness(
        MediaSettingsRange::create(std::move(capabilities->brightness)));
  }
  if (capabilities->contrast->max != capabilities->contrast->min) {
    m_capabilities.setContrast(
        MediaSettingsRange::create(std::move(capabilities->contrast)));
  }
  if (capabilities->saturation->max != capabilities->saturation->min) {
    m_capabilities.setSaturation(
        MediaSettingsRange::create(std::move(capabilities->saturation)));
  }
  if (capabilities->sharpness->max != capabilities->sharpness->min) {
    m_capabilities.setSharpness(
        MediaSettingsRange::create(std::move(capabilities->sharpness)));
  }

  if (capabilities->zoom->max != capabilities->zoom->min) {
    m_capabilities.setZoom(
        MediaSettingsRange::create(std::move(capabilities->zoom)));
  }

  // TODO(mcasas): do |torch| when the mojom interface is updated,
  // https://crbug.com/700607.
}

void ImageCapture::onServiceConnectionError() {
  m_service.reset();
  for (ScriptPromiseResolver* resolver : m_serviceRequests)
    resolver->reject(DOMException::create(NotFoundError, kNoServiceError));
  m_serviceRequests.clear();
}

DEFINE_TRACE(ImageCapture) {
  visitor->trace(m_streamTrack);
  visitor->trace(m_capabilities);
  visitor->trace(m_serviceRequests);
  EventTargetWithInlineData::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
