/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/mediastream/MediaConstraintsImpl.h"

#include "bindings/core/v8/ArrayValue.h"
#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/mediastream/MediaTrackConstraintSet.h"
#include "platform/Logging.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"

namespace blink {

namespace MediaConstraintsImpl {

// Legal constraint names.
// Temporary Note: Comments about source are where they are copied from.
// Once the chrome parts use the new-style constraint values, they will
// be deleted from the files mentioned.
// TODO(hta): remove comments before https://crbug.com/543997 is closed.

// From content/renderer/media/media_stream_video_source.cc
const char kMinAspectRatio[] = "minAspectRatio";
const char kMaxAspectRatio[] = "maxAspectRatio";
const char kMaxWidth[] = "maxWidth";
const char kMinWidth[] = "minWidth";
const char kMaxHeight[] = "maxHeight";
const char kMinHeight[] = "minHeight";
const char kMaxFrameRate[] = "maxFrameRate";
const char kMinFrameRate[] = "minFrameRate";
// From content/common/media/media_stream_options.cc
const char kMediaStreamSource[] = "chromeMediaSource";
const char kMediaStreamSourceId[] = "chromeMediaSourceId"; // mapped to deviceId
const char kMediaStreamSourceInfoId[] = "sourceId"; // mapped to deviceId
const char kMediaStreamRenderToAssociatedSink[] = "chromeRenderToAssociatedSink";
// RenderToAssociatedSink will be going away in M50-M60 some time.
const char kMediaStreamAudioHotword[] = "googHotword";
// TODO(hta): googHotword should go away. https://crbug.com/577627
// From content/renderer/media/media_stream_audio_processor_options.cc
const char kEchoCancellation[] = "echoCancellation";
const char kGoogEchoCancellation[] = "googEchoCancellation";
const char kGoogExperimentalEchoCancellation[] = "googEchoCancellation2";
const char kGoogAutoGainControl[] = "googAutoGainControl";
const char kGoogExperimentalAutoGainControl[] = "googAutoGainControl2";
const char kGoogNoiseSuppression[] = "googNoiseSuppression";
const char kGoogExperimentalNoiseSuppression[] = "googNoiseSuppression2";
const char kGoogBeamforming[] = "googBeamforming";
const char kGoogArrayGeometry[] = "googArrayGeometry";
const char kGoogHighpassFilter[] = "googHighpassFilter";
const char kGoogTypingNoiseDetection[] = "googTypingNoiseDetection";
const char kGoogAudioMirroring[] = "googAudioMirroring";

// From third_party/libjingle/source/talk/app/webrtc/mediaconstraintsinterface.cc
// Audio constraints.
// const char kExtendedFilterEchoCancellation[] = "googEchoCancellation2"; // duplicate k-name
const char kDAEchoCancellation[] = "googDAEchoCancellation";
// const char kNoiseSuppression[] = "googNoiseSuppression"; // duplicate k-name
// const char kExperimentalNoiseSuppression[] = "googNoiseSuppression2"; // duplicate k-name
// const char kHighpassFilter[] = "googHighpassFilter"; // duplicate k-name
// const char kTypingNoiseDetection[] = "googTypingNoiseDetection"; // duplicate k-name
// const char kAudioMirroring[] = "googAudioMirroring"; // duplicate k-name
const char kAecDump[] = "audioDebugRecording";

// Google-specific constraint keys for a local video source (getUserMedia).
const char kNoiseReduction[] = "googNoiseReduction";

// Constraint keys for CreateOffer / CreateAnswer defined in W3C specification.
const char kOfferToReceiveAudio[] = "OfferToReceiveAudio";
const char kOfferToReceiveVideo[] = "OfferToReceiveVideo";
const char kVoiceActivityDetection[] = "VoiceActivityDetection";
const char kIceRestart[] = "IceRestart";
// Google specific constraint for BUNDLE enable/disable.
const char kUseRtpMux[] = "googUseRtpMUX";
// Below constraints should be used during PeerConnection construction.
const char kEnableDtlsSrtp[] = "DtlsSrtpKeyAgreement";
const char kEnableRtpDataChannels[] = "RtpDataChannels";
// Google-specific constraint keys.
const char kEnableDscp[] = "googDscp";
const char kEnableIPv6[] = "googIPv6";
const char kEnableVideoSuspendBelowMinBitrate[] = "googSuspendBelowMinBitrate";
const char kNumUnsignalledRecvStreams[] = "googNumUnsignalledRecvStreams";
const char kCombinedAudioVideoBwe[] = "googCombinedAudioVideoBwe";
const char kScreencastMinBitrate[] = "googScreencastMinBitrate";
const char kCpuOveruseDetection[] = "googCpuOveruseDetection";
const char kCpuUnderuseThreshold[] = "googCpuUnderuseThreshold";
const char kCpuOveruseThreshold[] = "googCpuOveruseThreshold";
const char kCpuUnderuseEncodeRsdThreshold[] = "googCpuUnderuseEncodeRsdThreshold";
const char kCpuOveruseEncodeRsdThreshold[] = "googCpuOveruseEncodeRsdThreshold";
const char kCpuOveruseEncodeUsage[] = "googCpuOveruseEncodeUsage";
const char kHighStartBitrate[] = "googHighStartBitrate";
const char kPayloadPadding[] = "googPayloadPadding";
// End of names from libjingle
// Names that have been used in the past, but should now be ignored.
// Kept around for backwards compatibility.
// https://crbug.com/579729
const char kGoogLeakyBucket[] = "googLeakyBucket";

// Names used for testing.
const char kTestConstraint1[] = "valid_and_supported_1";
const char kTestConstraint2[] = "valid_and_supported_2";

static bool parseMandatoryConstraintsDictionary(const Dictionary& mandatoryConstraintsDictionary, WebVector<WebMediaConstraint>& mandatory)
{
    Vector<WebMediaConstraint> mandatoryConstraintsVector;
    HashMap<String, String> mandatoryConstraintsHashMap;
    bool ok = mandatoryConstraintsDictionary.getOwnPropertiesAsStringHashMap(mandatoryConstraintsHashMap);
    if (!ok)
        return false;

    for (const auto& iter : mandatoryConstraintsHashMap)
        mandatoryConstraintsVector.append(WebMediaConstraint(iter.key, iter.value));
    mandatory.assign(mandatoryConstraintsVector);
    return true;
}

static bool parseOptionalConstraintsVectorElement(const Dictionary& constraint, Vector<WebMediaConstraint>& optionalConstraintsVector)
{
    Vector<String> localNames;
    bool ok = constraint.getPropertyNames(localNames);
    if (!ok)
        return false;
    if (localNames.size() != 1)
        return false;
    const String& key = localNames[0];
    String value;
    ok = DictionaryHelper::get(constraint, key, value);
    if (!ok)
        return false;
    optionalConstraintsVector.append(WebMediaConstraint(key, value));
    return true;
}

// Old style parser. Deprecated.
static bool parse(const Dictionary& constraintsDictionary, WebVector<WebMediaConstraint>& optional, WebVector<WebMediaConstraint>& mandatory)
{
    if (constraintsDictionary.isUndefinedOrNull())
        return true;

    Vector<String> names;
    bool ok = constraintsDictionary.getPropertyNames(names);
    if (!ok)
        return false;

    String mandatoryName("mandatory");
    String optionalName("optional");

    for (Vector<String>::iterator it = names.begin(); it != names.end(); ++it) {
        if (*it != mandatoryName && *it != optionalName)
            return false;
    }

    if (names.contains(mandatoryName)) {
        Dictionary mandatoryConstraintsDictionary;
        bool ok = constraintsDictionary.get(mandatoryName, mandatoryConstraintsDictionary);
        if (!ok || mandatoryConstraintsDictionary.isUndefinedOrNull())
            return false;
        ok = parseMandatoryConstraintsDictionary(mandatoryConstraintsDictionary, mandatory);
        if (!ok)
            return false;
    }

    Vector<WebMediaConstraint> optionalConstraintsVector;
    if (names.contains(optionalName)) {
        ArrayValue optionalConstraints;
        bool ok = DictionaryHelper::get(constraintsDictionary, optionalName, optionalConstraints);
        if (!ok || optionalConstraints.isUndefinedOrNull())
            return false;

        size_t numberOfConstraints;
        ok = optionalConstraints.length(numberOfConstraints);
        if (!ok)
            return false;

        for (size_t i = 0; i < numberOfConstraints; ++i) {
            Dictionary constraint;
            ok = optionalConstraints.get(i, constraint);
            if (!ok || constraint.isUndefinedOrNull())
                return false;
            ok = parseOptionalConstraintsVectorElement(constraint, optionalConstraintsVector);
            if (!ok)
                return false;
        }
        optional.assign(optionalConstraintsVector);
    }

    return true;
}

static bool parse(const MediaTrackConstraintSet& constraintsIn, WebVector<WebMediaConstraint>& optional, WebVector<WebMediaConstraint>& mandatory)
{
    Vector<WebMediaConstraint> mandatoryConstraintsVector;
    if (constraintsIn.hasMandatory()) {
        bool ok = parseMandatoryConstraintsDictionary(constraintsIn.mandatory(), mandatory);
        if (!ok)
            return false;
    }

    Vector<WebMediaConstraint> optionalConstraintsVector;
    if (constraintsIn.hasOptional()) {
        const Vector<Dictionary>& optionalConstraints = constraintsIn.optional();

        for (const auto& constraint : optionalConstraints) {
            if (constraint.isUndefinedOrNull())
                return false;
            bool ok = parseOptionalConstraintsVectorElement(constraint, optionalConstraintsVector);
            if (!ok)
                return false;
        }
        optional.assign(optionalConstraintsVector);
    }
    return true;
}

static bool toBoolean(const WebString& asWebString)
{
    return asWebString.equals("true");
    // TODO(hta): Check against "false" and return error if it's neither.
    // https://crbug.com/576582
}

static void parseOldStyleNames(ExecutionContext* context, const WebVector<WebMediaConstraint>& oldNames, bool reportUnknownNames, WebMediaTrackConstraintSet& result, MediaErrorState& errorState)
{
    for (const WebMediaConstraint& constraint : oldNames) {
        if (constraint.m_name.equals(kMinAspectRatio)) {
            result.aspectRatio.setMin(atof(constraint.m_value.utf8().c_str()));
        } else if (constraint.m_name.equals(kMaxAspectRatio)) {
            result.aspectRatio.setMax(atof(constraint.m_value.utf8().c_str()));
        } else if (constraint.m_name.equals(kMaxWidth)) {
            result.width.setMax(atoi(constraint.m_value.utf8().c_str()));
        } else if (constraint.m_name.equals(kMinWidth)) {
            result.width.setMin(atoi(constraint.m_value.utf8().c_str()));
        } else if (constraint.m_name.equals(kMaxHeight)) {
            result.height.setMax(atoi(constraint.m_value.utf8().c_str()));
        } else if (constraint.m_name.equals(kMinHeight)) {
            result.height.setMin(atoi(constraint.m_value.utf8().c_str()));
        } else if (constraint.m_name.equals(kMinFrameRate)) {
            result.frameRate.setMin(atof(constraint.m_value.utf8().c_str()));
        } else if (constraint.m_name.equals(kMaxFrameRate)) {
            result.frameRate.setMax(atof(constraint.m_value.utf8().c_str()));
        } else if (constraint.m_name.equals(kEchoCancellation)) {
            result.echoCancellation.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kMediaStreamSource)) {
            // TODO(hta): This has only a few legal values. Should be
            // represented as an enum, and cause type errors.
            // https://crbug.com/576582
            result.mediaStreamSource.setExact(constraint.m_value);
        } else if (constraint.m_name.equals(kMediaStreamSourceId)
            || constraint.m_name.equals(kMediaStreamSourceInfoId)) {
            result.deviceId.setExact(constraint.m_value);
        } else if (constraint.m_name.equals(kMediaStreamRenderToAssociatedSink)) {
            // TODO(hta): This is a boolean represented as string.
            // Should give TypeError when it's not parseable.
            // https://crbug.com/576582
            result.renderToAssociatedSink.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kMediaStreamAudioHotword)) {
            result.hotwordEnabled.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kGoogEchoCancellation)) {
            result.googEchoCancellation.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kGoogExperimentalEchoCancellation)) {
            result.googExperimentalEchoCancellation.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kGoogAutoGainControl)) {
            result.googAutoGainControl.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kGoogExperimentalAutoGainControl)) {
            result.googExperimentalAutoGainControl.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kGoogNoiseSuppression)) {
            result.googNoiseSuppression.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kGoogExperimentalNoiseSuppression)) {
            result.googExperimentalNoiseSuppression.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kGoogBeamforming)) {
            result.googBeamforming.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kGoogArrayGeometry)) {
            result.googArrayGeometry.setExact(constraint.m_value);
        } else if (constraint.m_name.equals(kGoogHighpassFilter)) {
            result.googHighpassFilter.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kGoogTypingNoiseDetection)) {
            result.googTypingNoiseDetection.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kGoogAudioMirroring)) {
            result.googAudioMirroring.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kDAEchoCancellation)) {
            result.googDAEchoCancellation.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kAecDump)) {
            result.googAecDump.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kNoiseReduction)) {
            result.googNoiseReduction.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kOfferToReceiveAudio)) {
            result.offerToReceiveAudio.setExact(constraint.m_value);
        } else if (constraint.m_name.equals(kOfferToReceiveVideo)) {
            result.offerToReceiveVideo.setExact(constraint.m_value);
        } else if (constraint.m_name.equals(kVoiceActivityDetection)) {
            result.voiceActivityDetection.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kIceRestart)) {
            result.iceRestart.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kUseRtpMux)) {
            result.googUseRtpMux.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kEnableDtlsSrtp)) {
            result.enableDtlsSrtp.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kEnableRtpDataChannels)) {
            result.enableRtpDataChannels.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kEnableDscp)) {
            result.enableDscp.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kEnableIPv6)) {
            result.enableIPv6.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kEnableVideoSuspendBelowMinBitrate)) {
            result.googEnableVideoSuspendBelowMinBitrate.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kNumUnsignalledRecvStreams)) {
            result.googNumUnsignalledRecvStreams.setExact(atoi(constraint.m_value.utf8().c_str()));
        } else if (constraint.m_name.equals(kCombinedAudioVideoBwe)) {
            result.googCombinedAudioVideoBwe.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kScreencastMinBitrate)) {
            result.googScreencastMinBitrate.setExact(atoi(constraint.m_value.utf8().c_str()));
        } else if (constraint.m_name.equals(kCpuOveruseDetection)) {
            result.googCpuOveruseDetection.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kCpuUnderuseThreshold)) {
            result.googCpuUnderuseThreshold.setExact(atoi(constraint.m_value.utf8().c_str()));
        } else if (constraint.m_name.equals(kCpuOveruseThreshold)) {
            result.googCpuOveruseThreshold.setExact(atoi(constraint.m_value.utf8().c_str()));
        } else if (constraint.m_name.equals(kCpuUnderuseEncodeRsdThreshold)) {
            result.googCpuUnderuseEncodeRsdThreshold.setExact(atoi(constraint.m_value.utf8().c_str()));
        } else if (constraint.m_name.equals(kCpuOveruseEncodeRsdThreshold)) {
            result.googCpuOveruseEncodeRsdThreshold.setExact(atoi(constraint.m_value.utf8().c_str()));
        } else if (constraint.m_name.equals(kCpuOveruseEncodeUsage)) {
            result.googCpuOveruseEncodeUsage.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kHighStartBitrate)) {
            result.googHighStartBitrate.setExact(atoi(constraint.m_value.utf8().c_str()));
        } else if (constraint.m_name.equals(kPayloadPadding)) {
            result.googPayloadPadding.setExact(toBoolean(constraint.m_value));
        } else if (constraint.m_name.equals(kGoogLeakyBucket)) {
            context->addConsoleMessage(ConsoleMessage::create(DeprecationMessageSource, WarningMessageLevel,
                "Obsolete constraint named " + String(constraint.m_name)
                + " is ignored. Please stop using it."));
        } else if (constraint.m_name.equals(kTestConstraint1)
            || constraint.m_name.equals(kTestConstraint2)) {
            // These constraints are only for testing parsing.
            // Values 0 and 1 are legal, all others are a ConstraintError.
            if (!constraint.m_value.equals("0") && !constraint.m_value.equals("1")) {
                errorState.throwConstraintError("Illegal value for constraint", constraint.m_name);
            }
        } else {
            if (reportUnknownNames) {
                // TODO(hta): UMA stats for unknown constraints passed.
                // https://crbug.com/576613
                context->addConsoleMessage(
                    ConsoleMessage::create(
                        DeprecationMessageSource,
                        WarningMessageLevel,
                        "Unknown constraint named "
                        + String(constraint.m_name) + " rejected"));
                errorState.throwConstraintError("Unknown name of constraint detected", constraint.m_name);
            }
        }
    }
}

static WebMediaConstraints createFromNamedConstraints(ExecutionContext* context, WebVector<WebMediaConstraint>& mandatory, const WebVector<WebMediaConstraint>& optional, MediaErrorState& errorState)
{
    WebMediaTrackConstraintSet basic;
    WebMediaTrackConstraintSet advanced;
    WebMediaConstraints constraints;
    parseOldStyleNames(context, mandatory, true, basic, errorState);
    if (errorState.hadException())
        return constraints;
    // We ignore unknow names and syntax errors in optional constraints.
    MediaErrorState ignoredErrorState;
    parseOldStyleNames(context, optional, false, advanced, ignoredErrorState);
    WebVector<WebMediaTrackConstraintSet> advancedVector(&advanced, 1);
    // Use the 4-argument initializer until Chrome has been converted.
    constraints.initialize(optional, mandatory, basic, advancedVector);
    return constraints;
}

// Deprecated.
WebMediaConstraints create(ExecutionContext* context, const Dictionary& constraintsDictionary, MediaErrorState& errorState)
{
    WebVector<WebMediaConstraint> optional;
    WebVector<WebMediaConstraint> mandatory;
    if (!parse(constraintsDictionary, optional, mandatory)) {
        errorState.throwTypeError("Malformed constraints object.");
        return WebMediaConstraints();
    }
    UseCounter::count(context, UseCounter::MediaStreamConstraintsFromDictionary);
    return createFromNamedConstraints(context, mandatory, optional, errorState);
}

void copyLongConstraint(const ConstrainLongRange& blinkForm, LongConstraint& webForm)
{
    if (blinkForm.hasMin()) {
        webForm.setMin(blinkForm.min());
    }
    if (blinkForm.hasMax()) {
        webForm.setMax(blinkForm.max());
    }
    if (blinkForm.hasIdeal()) {
        webForm.setIdeal(blinkForm.ideal());
    }
    if (blinkForm.hasExact()) {
        webForm.setExact(blinkForm.exact());
    }
}

void copyDoubleConstraint(const ConstrainDoubleRange& blinkForm, DoubleConstraint& webForm)
{
    if (blinkForm.hasMin()) {
        webForm.setMin(blinkForm.min());
    }
    if (blinkForm.hasMax()) {
        webForm.setMax(blinkForm.max());
    }
    if (blinkForm.hasIdeal()) {
        webForm.setIdeal(blinkForm.ideal());
    }
    if (blinkForm.hasExact()) {
        webForm.setExact(blinkForm.exact());
    }
}

void copyStringConstraint(const ConstrainDOMStringParameters& blinkForm, StringConstraint& webForm)
{
    if (blinkForm.hasIdeal()) {
        webForm.setIdeal(WebVector<WebString>(blinkForm.ideal()));
    }
    if (blinkForm.hasExact()) {
        webForm.setExact(WebVector<WebString>(blinkForm.exact()));
    }
}

void copyBooleanConstraint(const ConstrainBooleanParameters& blinkForm, BooleanConstraint& webForm)
{
    if (blinkForm.hasIdeal()) {
        webForm.setIdeal(blinkForm.ideal());
    }
    if (blinkForm.hasExact()) {
        webForm.setExact(blinkForm.exact());
    }
}

void copyConstraints(const MediaTrackConstraintSet& constraintsIn, WebMediaTrackConstraintSet& constraintBuffer)
{
    if (constraintsIn.hasWidth()) {
        copyLongConstraint(constraintsIn.width(), constraintBuffer.width);
    }
    if (constraintsIn.hasHeight()) {
        copyLongConstraint(constraintsIn.height(), constraintBuffer.height);
    }
    if (constraintsIn.hasAspectRatio()) {
        copyDoubleConstraint(constraintsIn.aspectRatio(), constraintBuffer.aspectRatio);
    }
    if (constraintsIn.hasFrameRate()) {
        copyDoubleConstraint(constraintsIn.frameRate(), constraintBuffer.frameRate);
    }
    if (constraintsIn.hasFacingMode()) {
        copyStringConstraint(constraintsIn.facingMode(), constraintBuffer.facingMode);
    }
    if (constraintsIn.hasVolume()) {
        copyDoubleConstraint(constraintsIn.volume(), constraintBuffer.volume);
    }
    if (constraintsIn.hasSampleRate()) {
        copyLongConstraint(constraintsIn.sampleRate(), constraintBuffer.sampleRate);
    }
    if (constraintsIn.hasSampleSize()) {
        copyLongConstraint(constraintsIn.sampleSize(), constraintBuffer.sampleSize);
    }
    if (constraintsIn.hasEchoCancellation()) {
        copyBooleanConstraint(constraintsIn.echoCancellation(), constraintBuffer.echoCancellation);
    }
    if (constraintsIn.hasLatency()) {
        copyDoubleConstraint(constraintsIn.latency(), constraintBuffer.latency);
    }
    if (constraintsIn.hasChannelCount()) {
        copyLongConstraint(constraintsIn.channelCount(), constraintBuffer.channelCount);
    }
    if (constraintsIn.hasDeviceId()) {
        copyStringConstraint(constraintsIn.deviceId(), constraintBuffer.deviceId);
    }
    if (constraintsIn.hasGroupId()) {
        copyStringConstraint(constraintsIn.groupId(), constraintBuffer.groupId);
    }
}

WebMediaConstraints create(ExecutionContext* context, const MediaTrackConstraintSet& constraintsIn, MediaErrorState& errorState)
{
    WebMediaConstraints constraints;
    WebMediaTrackConstraintSet constraintBuffer;
    WebVector<WebMediaTrackConstraintSet> advancedBuffer;
    copyConstraints(constraintsIn, constraintBuffer);
    // TODO(hta): Add initialization of advanced constraints once present.
    // https://crbug.com/253412
    if (constraintsIn.hasOptional() || constraintsIn.hasMandatory()) {
        if (!constraintBuffer.isEmpty()) {
            errorState.throwTypeError("Malformed constraint: Cannot use both optional/mandatory and specific constraints.");
            return WebMediaConstraints();
        }
        WebVector<WebMediaConstraint> optional;
        WebVector<WebMediaConstraint> mandatory;
        if (!parse(constraintsIn, optional, mandatory)) {
            errorState.throwTypeError("Malformed constraints object.");
            return WebMediaConstraints();
        }
        UseCounter::count(context, UseCounter::MediaStreamConstraintsNameValue);
        return createFromNamedConstraints(context, mandatory, optional, errorState);
    }
    UseCounter::count(context, UseCounter::MediaStreamConstraintsConformant);
    constraints.initialize(constraintBuffer, advancedBuffer);
    return constraints;
}

WebMediaConstraints create()
{
    WebMediaConstraints constraints;
    constraints.initialize();
    return constraints;
}

} // namespace MediaConstraintsImpl
} // namespace blink
