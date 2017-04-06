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
#include "modules/mediastream/MediaTrackConstraints.h"
#include "platform/WaitableEvent.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/WebImageCaptureFrameGrabber.h"
#include "public/platform/WebMediaStreamTrack.h"
#include "wtf/PtrUtil.h"

namespace blink {

using FillLightMode = media::mojom::blink::FillLightMode;
using MeteringMode = media::mojom::blink::MeteringMode;

namespace {

const char kNoServiceError[] = "ImageCapture service unavailable.";

bool trackIsInactive(const MediaStreamTrack& track) {
  // Spec instructs to return an exception if the Track's readyState() is not
  // "live". Also reject if the track is disabled or muted.
  return track.readyState() != "live" || !track.enabled() || track.muted();
}

MeteringMode parseMeteringMode(const String& blinkMode) {
  if (blinkMode == "manual")
    return MeteringMode::MANUAL;
  if (blinkMode == "single-shot")
    return MeteringMode::SINGLE_SHOT;
  if (blinkMode == "continuous")
    return MeteringMode::CONTINUOUS;
  if (blinkMode == "none")
    return MeteringMode::NONE;
  NOTREACHED();
  return MeteringMode::NONE;
}

FillLightMode parseFillLightMode(const String& blinkMode) {
  if (blinkMode == "off")
    return FillLightMode::OFF;
  if (blinkMode == "auto")
    return FillLightMode::AUTO;
  if (blinkMode == "flash")
    return FillLightMode::FLASH;
  NOTREACHED();
  return FillLightMode::OFF;
}

WebString toString(MeteringMode value) {
  switch (value) {
    case MeteringMode::NONE:
      return WebString::fromUTF8("none");
    case MeteringMode::MANUAL:
      return WebString::fromUTF8("manual");
    case MeteringMode::SINGLE_SHOT:
      return WebString::fromUTF8("single-shot");
    case MeteringMode::CONTINUOUS:
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

ScriptPromise ImageCapture::getPhotoCapabilities(ScriptState* scriptState) {
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
      convertToBaseCallback(WTF::bind(&ImageCapture::onPhotoCapabilities,
                                      wrapPersistent(this),
                                      wrapPersistent(resolver))));
  return promise;
}

ScriptPromise ImageCapture::setOptions(ScriptState* scriptState,
                                       const PhotoSettings& photoSettings) {
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
  auto settings = media::mojom::blink::PhotoSettings::New();

  settings->has_height = photoSettings.hasImageHeight();
  if (settings->has_height)
    settings->height = photoSettings.imageHeight();
  settings->has_width = photoSettings.hasImageWidth();
  if (settings->has_width)
    settings->width = photoSettings.imageWidth();

  settings->has_red_eye_reduction = photoSettings.hasRedEyeReduction();
  if (settings->has_red_eye_reduction)
    settings->red_eye_reduction = photoSettings.redEyeReduction();
  settings->has_fill_light_mode = photoSettings.hasFillLightMode();
  if (settings->has_fill_light_mode) {
    settings->fill_light_mode =
        parseFillLightMode(photoSettings.fillLightMode());
  }

  m_service->SetOptions(m_streamTrack->component()->source()->id(),
                        std::move(settings),
                        convertToBaseCallback(WTF::bind(
                            &ImageCapture::onSetOptions, wrapPersistent(this),
                            wrapPersistent(resolver))));
  return promise;
}

ScriptPromise ImageCapture::takePhoto(ScriptState* scriptState) {
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

ScriptPromise ImageCapture::grabFrame(ScriptState* scriptState) {
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

// TODO(mcasas): make the implementation fully Spec compliant, see the TODOs
// inside the method, https://crbug.com/708723.
void ImageCapture::setMediaTrackConstraints(
    ScriptPromiseResolver* resolver,
    const HeapVector<MediaTrackConstraintSet>& constraintsVector) {
  if (!m_service) {
    resolver->reject(DOMException::create(NotFoundError, kNoServiceError));
    return;
  }
  m_serviceRequests.insert(resolver);

  // TODO(mcasas): add support more than one single advanced constraint.
  const auto constraints = constraintsVector[0];

  auto settings = media::mojom::blink::PhotoSettings::New();

  // TODO(mcasas): support other Mode types beyond simple string i.e. the
  // equivalents of "sequence<DOMString>"" or "ConstrainDOMStringParameters".
  settings->has_white_balance_mode = constraints.hasWhiteBalanceMode() &&
                                     constraints.whiteBalanceMode().isString();
  if (settings->has_white_balance_mode) {
    m_currentConstraints.setWhiteBalanceMode(constraints.whiteBalanceMode());
    settings->white_balance_mode =
        parseMeteringMode(constraints.whiteBalanceMode().getAsString());
  }
  settings->has_exposure_mode =
      constraints.hasExposureMode() && constraints.exposureMode().isString();
  if (settings->has_exposure_mode) {
    m_currentConstraints.setExposureMode(constraints.exposureMode());
    settings->exposure_mode =
        parseMeteringMode(constraints.exposureMode().getAsString());
  }

  settings->has_focus_mode =
      constraints.hasFocusMode() && constraints.focusMode().isString();
  if (settings->has_focus_mode) {
    m_currentConstraints.setFocusMode(constraints.focusMode());
    settings->focus_mode =
        parseMeteringMode(constraints.focusMode().getAsString());
  }

  // TODO(mcasas): support ConstrainPoint2DParameters.
  if (constraints.hasPointsOfInterest() &&
      constraints.pointsOfInterest().isPoint2DSequence()) {
    for (const auto& point :
         constraints.pointsOfInterest().getAsPoint2DSequence()) {
      auto mojoPoint = media::mojom::blink::Point2D::New();
      mojoPoint->x = point.x();
      mojoPoint->y = point.y();
      settings->points_of_interest.push_back(std::move(mojoPoint));
    }
    m_currentConstraints.setPointsOfInterest(constraints.pointsOfInterest());
  }

  // TODO(mcasas): support ConstrainDoubleRange where applicable.
  settings->has_exposure_compensation =
      constraints.hasExposureCompensation() &&
      constraints.exposureCompensation().isDouble();
  if (settings->has_exposure_compensation) {
    m_currentConstraints.setExposureCompensation(
        constraints.exposureCompensation());
    settings->exposure_compensation =
        constraints.exposureCompensation().getAsDouble();
  }
  settings->has_color_temperature = constraints.hasColorTemperature() &&
                                    constraints.colorTemperature().isDouble();
  if (settings->has_color_temperature) {
    m_currentConstraints.setColorTemperature(constraints.colorTemperature());
    settings->color_temperature = constraints.colorTemperature().getAsDouble();
  }
  settings->has_iso = constraints.hasIso() && constraints.iso().isDouble();
  if (settings->has_iso) {
    m_currentConstraints.setIso(constraints.iso());
    settings->iso = constraints.iso().getAsDouble();
  }

  settings->has_brightness =
      constraints.hasBrightness() && constraints.brightness().isDouble();
  if (settings->has_brightness) {
    m_currentConstraints.setBrightness(constraints.brightness());
    settings->brightness = constraints.brightness().getAsDouble();
  }
  settings->has_contrast =
      constraints.hasContrast() && constraints.contrast().isDouble();
  if (settings->has_contrast) {
    m_currentConstraints.setContrast(constraints.contrast());
    settings->contrast = constraints.contrast().getAsDouble();
  }
  settings->has_saturation =
      constraints.hasSaturation() && constraints.saturation().isDouble();
  if (settings->has_saturation) {
    m_currentConstraints.setSaturation(constraints.saturation());
    settings->saturation = constraints.saturation().getAsDouble();
  }
  settings->has_sharpness =
      constraints.hasSharpness() && constraints.sharpness().isDouble();
  if (settings->has_sharpness) {
    m_currentConstraints.setSharpness(constraints.sharpness());
    settings->sharpness = constraints.sharpness().getAsDouble();
  }

  settings->has_zoom = constraints.hasZoom() && constraints.zoom().isDouble();
  if (settings->has_zoom) {
    m_currentConstraints.setZoom(constraints.zoom());
    settings->zoom = constraints.zoom().getAsDouble();
  }

  // TODO(mcasas): support ConstrainBooleanParameters where applicable.
  settings->has_torch =
      constraints.hasTorch() && constraints.torch().isBoolean();
  if (settings->has_torch) {
    m_currentConstraints.setTorch(constraints.torch());
    settings->torch = constraints.torch().getAsBoolean();
  }

  m_service->SetOptions(m_streamTrack->component()->source()->id(),
                        std::move(settings),
                        convertToBaseCallback(WTF::bind(
                            &ImageCapture::onSetOptions, wrapPersistent(this),
                            wrapPersistent(resolver))));
}

const MediaTrackConstraintSet& ImageCapture::getMediaTrackConstraints() const {
  return m_currentConstraints;
}

void ImageCapture::clearMediaTrackConstraints(ScriptPromiseResolver* resolver) {
  m_currentConstraints = MediaTrackConstraintSet();
  resolver->resolve();

  // TODO(mcasas): Clear also any PhotoSettings that the device might have got
  // configured, for that we need to know a "default" state of the device; take
  // a snapshot upon first opening. https://crbug.com/700607.
}

void ImageCapture::getMediaTrackSettings(MediaTrackSettings& settings) const {
  // Merge any present |m_settings| members into |settings|.

  if (m_settings.hasWhiteBalanceMode())
    settings.setWhiteBalanceMode(m_settings.whiteBalanceMode());
  if (m_settings.hasExposureMode())
    settings.setExposureMode(m_settings.exposureMode());
  if (m_settings.hasFocusMode())
    settings.setFocusMode(m_settings.focusMode());

  if (m_settings.hasPointsOfInterest())
    settings.setPointsOfInterest(m_settings.pointsOfInterest());

  if (m_settings.hasExposureCompensation())
    settings.setExposureCompensation(m_settings.exposureCompensation());
  if (m_settings.hasColorTemperature())
    settings.setColorTemperature(m_settings.colorTemperature());
  if (m_settings.hasIso())
    settings.setIso(m_settings.iso());

  if (m_settings.hasBrightness())
    settings.setBrightness(m_settings.brightness());
  if (m_settings.hasContrast())
    settings.setContrast(m_settings.contrast());
  if (m_settings.hasSaturation())
    settings.setSaturation(m_settings.saturation());
  if (m_settings.hasSharpness())
    settings.setSharpness(m_settings.sharpness());

  if (m_settings.hasZoom())
    settings.setZoom(m_settings.zoom());
  if (m_settings.hasTorch())
    settings.setTorch(m_settings.torch());
}

bool ImageCapture::hasNonImageCaptureConstraints(
    const MediaTrackConstraints& constraints) const {
  if (!constraints.hasAdvanced())
    return false;

  const auto& advancedConstraints = constraints.advanced();
  for (const auto& constraint : advancedConstraints) {
    if (constraint.hasWidth() || constraint.hasHeight() ||
        constraint.hasAspectRatio() || constraint.hasFrameRate() ||
        constraint.hasFacingMode() || constraint.hasVolume() ||
        constraint.hasSampleRate() || constraint.hasSampleSize() ||
        constraint.hasEchoCancellation() || constraint.hasLatency() ||
        constraint.hasChannelCount() || constraint.hasDeviceId() ||
        constraint.hasGroupId() || constraint.hasVideoKind() ||
        constraint.hasDepthNear() || constraint.hasDepthFar() ||
        constraint.hasFocalLengthX() || constraint.hasFocalLengthY() ||
        constraint.hasMandatory() || constraint.hasOptional()) {
      return true;
    }
  }
  return false;
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
      convertToBaseCallback(WTF::bind(&ImageCapture::onCapabilitiesUpdate,
                                      wrapPersistent(this))));
}

void ImageCapture::onPhotoCapabilities(
    ScriptPromiseResolver* resolver,
    media::mojom::blink::PhotoCapabilitiesPtr capabilities) {
  if (!m_serviceRequests.contains(resolver))
    return;
  if (capabilities.is_null()) {
    resolver->reject(DOMException::create(UnknownError, "platform error"));
  } else {
    // Update the local capabilities cache.
    onCapabilitiesUpdateInternal(*capabilities);

    PhotoCapabilities* caps = PhotoCapabilities::create();

    caps->setRedEyeReduction(capabilities->red_eye_reduction);
    // TODO(mcasas): Remove the explicit MediaSettingsRange::create() when
    // mojo::StructTraits supports garbage-collected mappings,
    // https://crbug.com/700180.
    caps->setImageHeight(
        MediaSettingsRange::create(std::move(capabilities->height)));
    caps->setImageWidth(
        MediaSettingsRange::create(std::move(capabilities->width)));
    caps->setFillLightMode(capabilities->fill_light_mode);

    resolver->resolve(caps);
  }
  m_serviceRequests.erase(resolver);
}

void ImageCapture::onSetOptions(ScriptPromiseResolver* resolver, bool result) {
  if (!m_serviceRequests.contains(resolver))
    return;

  if (!result) {
    resolver->reject(DOMException::create(UnknownError, "setOptions failed"));
    m_serviceRequests.erase(resolver);
    return;
  }

  // Retrieve the current device status after setting the options.
  m_service->GetCapabilities(
      m_streamTrack->component()->source()->id(),
      convertToBaseCallback(WTF::bind(&ImageCapture::onPhotoCapabilities,
                                      wrapPersistent(this),
                                      wrapPersistent(resolver))));
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

void ImageCapture::onCapabilitiesUpdate(
    media::mojom::blink::PhotoCapabilitiesPtr capabilities) {
  if (!capabilities.is_null())
    onCapabilitiesUpdateInternal(*capabilities);
}

void ImageCapture::onCapabilitiesUpdateInternal(
    const media::mojom::blink::PhotoCapabilities& capabilities) {
  // TODO(mcasas): adapt the mojo interface to return a list of supported Modes
  // when moving these out of PhotoCapabilities, https://crbug.com/700607.
  m_capabilities.setWhiteBalanceMode(
      WTF::Vector<WTF::String>({toString(capabilities.white_balance_mode)}));
  if (m_capabilities.hasWhiteBalanceMode())
    m_settings.setWhiteBalanceMode(m_capabilities.whiteBalanceMode()[0]);
  m_capabilities.setExposureMode(
      WTF::Vector<WTF::String>({toString(capabilities.exposure_mode)}));
  if (m_capabilities.hasExposureMode())
    m_settings.setExposureMode(m_capabilities.exposureMode()[0]);
  m_capabilities.setFocusMode(
      WTF::Vector<WTF::String>({toString(capabilities.focus_mode)}));
  if (m_capabilities.hasFocusMode())
    m_settings.setFocusMode(m_capabilities.focusMode()[0]);

  HeapVector<Point2D> currentPointsOfInterest;
  if (!capabilities.points_of_interest.isEmpty()) {
    for (const auto& point : capabilities.points_of_interest) {
      Point2D webPoint;
      webPoint.setX(point->x);
      webPoint.setY(point->y);
      currentPointsOfInterest.push_back(mojo::Clone(webPoint));
    }
  }
  m_settings.setPointsOfInterest(std::move(currentPointsOfInterest));

  // TODO(mcasas): Remove the explicit MediaSettingsRange::create() when
  // mojo::StructTraits supports garbage-collected mappings,
  // https://crbug.com/700180.
  if (capabilities.exposure_compensation->max !=
      capabilities.exposure_compensation->min) {
    m_capabilities.setExposureCompensation(
        MediaSettingsRange::create(*capabilities.exposure_compensation));
    m_settings.setExposureCompensation(
        capabilities.exposure_compensation->current);
  }
  if (capabilities.color_temperature->max !=
      capabilities.color_temperature->min) {
    m_capabilities.setColorTemperature(
        MediaSettingsRange::create(*capabilities.color_temperature));
    m_settings.setColorTemperature(capabilities.color_temperature->current);
  }
  if (capabilities.iso->max != capabilities.iso->min) {
    m_capabilities.setIso(MediaSettingsRange::create(*capabilities.iso));
    m_settings.setIso(capabilities.iso->current);
  }

  if (capabilities.brightness->max != capabilities.brightness->min) {
    m_capabilities.setBrightness(
        MediaSettingsRange::create(*capabilities.brightness));
    m_settings.setBrightness(capabilities.brightness->current);
  }
  if (capabilities.contrast->max != capabilities.contrast->min) {
    m_capabilities.setContrast(
        MediaSettingsRange::create(*capabilities.contrast));
    m_settings.setContrast(capabilities.contrast->current);
  }
  if (capabilities.saturation->max != capabilities.saturation->min) {
    m_capabilities.setSaturation(
        MediaSettingsRange::create(*capabilities.saturation));
    m_settings.setSaturation(capabilities.saturation->current);
  }
  if (capabilities.sharpness->max != capabilities.sharpness->min) {
    m_capabilities.setSharpness(
        MediaSettingsRange::create(*capabilities.sharpness));
    m_settings.setSharpness(capabilities.sharpness->current);
  }

  if (capabilities.zoom->max != capabilities.zoom->min) {
    m_capabilities.setZoom(MediaSettingsRange::create(*capabilities.zoom));
    m_settings.setZoom(capabilities.zoom->current);
  }

  m_capabilities.setTorch(capabilities.torch);

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
  visitor->trace(m_settings);
  visitor->trace(m_currentConstraints);
  visitor->trace(m_serviceRequests);
  EventTargetWithInlineData::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
