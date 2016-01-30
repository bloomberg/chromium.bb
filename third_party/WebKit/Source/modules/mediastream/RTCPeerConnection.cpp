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

#include "modules/mediastream/RTCPeerConnection.h"

#include "bindings/core/v8/ArrayValue.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/Nullable.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/modules/v8/V8RTCCertificate.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/VoidCallback.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "modules/crypto/CryptoResultImpl.h"
#include "modules/mediastream/MediaConstraintsImpl.h"
#include "modules/mediastream/MediaStreamEvent.h"
#include "modules/mediastream/RTCDTMFSender.h"
#include "modules/mediastream/RTCDataChannel.h"
#include "modules/mediastream/RTCDataChannelEvent.h"
#include "modules/mediastream/RTCErrorCallback.h"
#include "modules/mediastream/RTCIceCandidateEvent.h"
#include "modules/mediastream/RTCSessionDescription.h"
#include "modules/mediastream/RTCSessionDescriptionCallback.h"
#include "modules/mediastream/RTCSessionDescriptionRequestImpl.h"
#include "modules/mediastream/RTCStatsCallback.h"
#include "modules/mediastream/RTCStatsRequestImpl.h"
#include "modules/mediastream/RTCVoidRequestImpl.h"
#include "platform/mediastream/RTCConfiguration.h"
#include "platform/mediastream/RTCOfferOptions.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCryptoAlgorithmParams.h"
#include "public/platform/WebCryptoUtil.h"
#include "public/platform/WebMediaStream.h"
#include "public/platform/WebRTCCertificate.h"
#include "public/platform/WebRTCCertificateGenerator.h"
#include "public/platform/WebRTCConfiguration.h"
#include "public/platform/WebRTCDataChannelHandler.h"
#include "public/platform/WebRTCDataChannelInit.h"
#include "public/platform/WebRTCICECandidate.h"
#include "public/platform/WebRTCKeyParams.h"
#include "public/platform/WebRTCOfferOptions.h"
#include "public/platform/WebRTCSessionDescription.h"
#include "public/platform/WebRTCSessionDescriptionRequest.h"
#include "public/platform/WebRTCStatsRequest.h"
#include "public/platform/WebRTCVoidRequest.h"

namespace blink {

namespace {

static bool throwExceptionIfSignalingStateClosed(RTCPeerConnection::SignalingState state, ExceptionState& exceptionState)
{
    if (state == RTCPeerConnection::SignalingStateClosed) {
        exceptionState.throwDOMException(InvalidStateError, "The RTCPeerConnection's signalingState is 'closed'.");
        return true;
    }

    return false;
}

// Helper class for RTCPeerConnection::generateCertificate.
class WebRTCCertificateObserver : public WebCallbacks<WebRTCCertificate*, void> {
public:
    // The created observer is responsible for deleting itself after onSuccess/onError. To avoid memory
    // leak the observer should therefore be used in a WebRTCCertificateGenerator::generateCertificate call
    // which is ensured to invoke one of these. Takes ownership of |resolver|.
    static WebRTCCertificateObserver* create(ScriptPromiseResolver* resolver)
    {
        return new WebRTCCertificateObserver(resolver);
    }

    DEFINE_INLINE_TRACE() { visitor->trace(m_resolver); }

private:
    WebRTCCertificateObserver(ScriptPromiseResolver* resolver)
        : m_resolver(resolver) {}

    ~WebRTCCertificateObserver() override {}

    void onSuccess(WebRTCCertificate* certificate) override
    {
        m_resolver->resolve(new RTCCertificate(certificate));
        delete this;
    }

    void onError() override
    {
        m_resolver->reject();
        delete this;
    }

    Persistent<ScriptPromiseResolver> m_resolver;
};

} // namespace

RTCPeerConnection::EventWrapper::EventWrapper(
    PassRefPtrWillBeRawPtr<Event> event,
    PassOwnPtr<BoolFunction> function)
    : m_event(event)
    , m_setupFunction(function)
{
}

bool RTCPeerConnection::EventWrapper::setup()
{
    if (m_setupFunction) {
        return (*m_setupFunction)();
    }
    return true;
}

DEFINE_TRACE(RTCPeerConnection::EventWrapper)
{
    visitor->trace(m_event);
}

RTCConfiguration* RTCPeerConnection::parseConfiguration(const Dictionary& configuration, ExceptionState& exceptionState)
{
    if (configuration.isUndefinedOrNull())
        return 0;

    RTCIceTransports iceTransports = RTCIceTransportsAll;
    String iceTransportsString;
    if (DictionaryHelper::get(configuration, "iceTransports", iceTransportsString)) {
        if (iceTransportsString == "none") {
            iceTransports = RTCIceTransportsNone;
        } else if (iceTransportsString == "relay") {
            iceTransports = RTCIceTransportsRelay;
        } else if (iceTransportsString != "all") {
            exceptionState.throwTypeError("Malformed RTCIceTransports");
            return 0;
        }
    }

    ArrayValue iceServers;
    bool ok = DictionaryHelper::get(configuration, "iceServers", iceServers);
    if (!ok || iceServers.isUndefinedOrNull()) {
        exceptionState.throwTypeError("Malformed RTCConfiguration");
        return 0;
    }

    size_t numberOfServers;
    ok = iceServers.length(numberOfServers);
    if (!ok) {
        exceptionState.throwTypeError("Malformed RTCConfiguration");
        return 0;
    }

    RTCBundlePolicy bundlePolicy = RTCBundlePolicyBalanced;
    String bundlePolicyString;
    if (DictionaryHelper::get(configuration, "bundlePolicy", bundlePolicyString)) {
        if (bundlePolicyString == "max-compat") {
            bundlePolicy = RTCBundlePolicyMaxCompat;
        } else if (bundlePolicyString == "max-bundle") {
            bundlePolicy = RTCBundlePolicyMaxBundle;
        } else if (bundlePolicyString != "balanced") {
            exceptionState.throwTypeError("Malformed RTCBundlePolicy");
            return 0;
        }
    }

    RTCRtcpMuxPolicy rtcpMuxPolicy = RTCRtcpMuxPolicyNegotiate;
    String rtcpMuxPolicyString;
    if (DictionaryHelper::get(configuration, "rtcpMuxPolicy", rtcpMuxPolicyString)) {
        if (rtcpMuxPolicyString == "require") {
            rtcpMuxPolicy = RTCRtcpMuxPolicyRequire;
        } else if (rtcpMuxPolicyString != "negotiate") {
            exceptionState.throwTypeError("Malformed RTCRtcpMuxPolicy");
            return 0;
        }
    }

    RTCConfiguration* rtcConfiguration = RTCConfiguration::create();
    rtcConfiguration->setIceTransports(iceTransports);
    rtcConfiguration->setBundlePolicy(bundlePolicy);
    rtcConfiguration->setRtcpMuxPolicy(rtcpMuxPolicy);

    for (size_t i = 0; i < numberOfServers; ++i) {
        Dictionary iceServer;
        ok = iceServers.get(i, iceServer);
        if (!ok) {
            exceptionState.throwTypeError("Malformed RTCIceServer");
            return 0;
        }

        Vector<String> names;
        iceServer.getPropertyNames(names);

        Vector<String> urlStrings;
        if (names.contains("urls")) {
            if (!DictionaryHelper::get(iceServer, "urls", urlStrings) || !urlStrings.size()) {
                String urlString;
                if (DictionaryHelper::get(iceServer, "urls", urlString)) {
                    urlStrings.append(urlString);
                } else {
                    exceptionState.throwTypeError("Malformed RTCIceServer");
                    return 0;
                }
            }
        } else if (names.contains("url")) {
            String urlString;
            if (DictionaryHelper::get(iceServer, "url", urlString)) {
                urlStrings.append(urlString);
            } else {
                exceptionState.throwTypeError("Malformed RTCIceServer");
                return 0;
            }
        } else {
            exceptionState.throwTypeError("Malformed RTCIceServer");
            return 0;
        }

        String username, credential;
        DictionaryHelper::get(iceServer, "username", username);
        DictionaryHelper::get(iceServer, "credential", credential);

        for (Vector<String>::iterator iter = urlStrings.begin(); iter != urlStrings.end(); ++iter) {
            KURL url(KURL(), *iter);
            if (!url.isValid() || !(url.protocolIs("turn") || url.protocolIs("turns") || url.protocolIs("stun"))) {
                exceptionState.throwTypeError("Malformed URL");
                return 0;
            }

            rtcConfiguration->appendServer(RTCIceServer::create(url, username, credential));
        }
    }

    ArrayValue certificates;
    if (DictionaryHelper::get(configuration, "certificates", certificates) && !certificates.isUndefinedOrNull()) {
        size_t numberOfCertificates;
        certificates.length(numberOfCertificates);
        for (size_t i = 0; i < numberOfCertificates; ++i) {
            RTCCertificate* certificate = nullptr;

            Dictionary dictCert;
            certificates.get(i, dictCert);
            v8::Local<v8::Value> valCert = dictCert.v8Value();
            if (!valCert.IsEmpty()) {
                certificate = V8RTCCertificate::toImplWithTypeCheck(configuration.isolate(), valCert);
            }
            if (!certificate) {
                exceptionState.throwTypeError("Malformed sequence<RTCCertificate>");
                return 0;
            }

            rtcConfiguration->appendCertificate(certificate->certificateShallowCopy());
        }
    }

    return rtcConfiguration;
}

RTCOfferOptions* RTCPeerConnection::parseOfferOptions(const Dictionary& options, ExceptionState& exceptionState)
{
    if (options.isUndefinedOrNull())
        return 0;

    Vector<String> propertyNames;
    options.getPropertyNames(propertyNames);

    // Treat |options| as MediaConstraints if it is empty or has "optional" or "mandatory" properties for compatibility.
    // TODO(jiayl): remove constraints when RTCOfferOptions reaches Stable and client code is ready.
    if (propertyNames.isEmpty() || propertyNames.contains("optional") || propertyNames.contains("mandatory"))
        return 0;

    int32_t offerToReceiveVideo = -1;
    int32_t offerToReceiveAudio = -1;
    bool voiceActivityDetection = true;
    bool iceRestart = false;

    if (DictionaryHelper::get(options, "offerToReceiveVideo", offerToReceiveVideo) && offerToReceiveVideo < 0) {
        exceptionState.throwTypeError("Invalid offerToReceiveVideo");
        return 0;
    }

    if (DictionaryHelper::get(options, "offerToReceiveAudio", offerToReceiveAudio) && offerToReceiveAudio < 0) {
        exceptionState.throwTypeError("Invalid offerToReceiveAudio");
        return 0;
    }

    DictionaryHelper::get(options, "voiceActivityDetection", voiceActivityDetection);
    DictionaryHelper::get(options, "iceRestart", iceRestart);

    RTCOfferOptions* rtcOfferOptions = RTCOfferOptions::create(offerToReceiveVideo, offerToReceiveAudio, voiceActivityDetection, iceRestart);
    return rtcOfferOptions;
}

RTCPeerConnection* RTCPeerConnection::create(ExecutionContext* context, const Dictionary& rtcConfiguration, const Dictionary& mediaConstraints, ExceptionState& exceptionState)
{
    if (mediaConstraints.isObject())
        UseCounter::count(context, UseCounter::RTCPeerConnectionConstructorConstraints);
    else
        UseCounter::count(context, UseCounter::RTCPeerConnectionConstructorCompliant);

    RTCConfiguration* configuration = parseConfiguration(rtcConfiguration, exceptionState);
    if (exceptionState.hadException())
        return 0;

    MediaErrorState mediaErrorState;
    WebMediaConstraints constraints = MediaConstraintsImpl::create(context, mediaConstraints, mediaErrorState);
    if (mediaErrorState.hadException()) {
        mediaErrorState.raiseException(exceptionState);
        return 0;
    }

    RTCPeerConnection* peerConnection = new RTCPeerConnection(context, configuration, constraints, exceptionState);
    peerConnection->suspendIfNeeded();
    if (exceptionState.hadException())
        return 0;

    return peerConnection;
}

RTCPeerConnection::RTCPeerConnection(ExecutionContext* context, RTCConfiguration* configuration, WebMediaConstraints constraints, ExceptionState& exceptionState)
    : ActiveDOMObject(context)
    , m_signalingState(SignalingStateStable)
    , m_iceGatheringState(ICEGatheringStateNew)
    , m_iceConnectionState(ICEConnectionStateNew)
    , m_dispatchScheduledEventRunner(AsyncMethodRunner<RTCPeerConnection>::create(this, &RTCPeerConnection::dispatchScheduledEvent))
    , m_stopped(false)
    , m_closed(false)
{
    Document* document = toDocument(executionContext());

    // If we fail, set |m_closed| and |m_stopped| to true, to avoid hitting the assert in the destructor.

    if (!document->frame()) {
        m_closed = true;
        m_stopped = true;
        exceptionState.throwDOMException(NotSupportedError, "PeerConnections may not be created in detached documents.");
        return;
    }

    m_peerHandler = adoptPtr(Platform::current()->createRTCPeerConnectionHandler(this));
    if (!m_peerHandler) {
        m_closed = true;
        m_stopped = true;
        exceptionState.throwDOMException(NotSupportedError, "No PeerConnection handler can be created, perhaps WebRTC is disabled?");
        return;
    }

    document->frame()->loader().client()->dispatchWillStartUsingPeerConnectionHandler(m_peerHandler.get());

    if (!m_peerHandler->initialize(configuration, constraints)) {
        m_closed = true;
        m_stopped = true;
        exceptionState.throwDOMException(NotSupportedError, "Failed to initialize native PeerConnection.");
        return;
    }
}

RTCPeerConnection::~RTCPeerConnection()
{
    // This checks that close() or stop() is called before the destructor.
    // We are assuming that a wrapper is always created when RTCPeerConnection is created.
    ASSERT(m_closed || m_stopped);
}

void RTCPeerConnection::createOffer(ExecutionContext* context, RTCSessionDescriptionCallback* successCallback, RTCErrorCallback* errorCallback, const Dictionary& rtcOfferOptions, ExceptionState& exceptionState)
{
    if (errorCallback)
        UseCounter::count(context, UseCounter::RTCPeerConnectionCreateOfferLegacyFailureCallback);
    else
        UseCounter::countDeprecation(context, UseCounter::RTCPeerConnectionCreateOfferLegacyNoFailureCallback);

    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    ASSERT(successCallback);

    RTCOfferOptions* offerOptions = parseOfferOptions(rtcOfferOptions, exceptionState);
    if (exceptionState.hadException())
        return;

    RTCSessionDescriptionRequest* request = RTCSessionDescriptionRequestImpl::create(executionContext(), this, successCallback, errorCallback);

    if (offerOptions) {
        if (offerOptions->offerToReceiveAudio() != -1 || offerOptions->offerToReceiveVideo() != -1)
            UseCounter::count(context, UseCounter::RTCPeerConnectionCreateOfferLegacyOfferOptions);
        else if (errorCallback)
            UseCounter::count(context, UseCounter::RTCPeerConnectionCreateOfferLegacyCompliant);

        m_peerHandler->createOffer(request, offerOptions);
    } else {
        MediaErrorState mediaErrorState;
        WebMediaConstraints constraints = MediaConstraintsImpl::create(context, rtcOfferOptions, mediaErrorState);
        if (mediaErrorState.hadException()) {
            mediaErrorState.raiseException(exceptionState);
            return;
        }

        if (!constraints.isEmpty())
            UseCounter::count(context, UseCounter::RTCPeerConnectionCreateOfferLegacyConstraints);
        else if (errorCallback)
            UseCounter::count(context, UseCounter::RTCPeerConnectionCreateOfferLegacyCompliant);

        m_peerHandler->createOffer(request, constraints);
    }
}

void RTCPeerConnection::createAnswer(ExecutionContext* context, RTCSessionDescriptionCallback* successCallback, RTCErrorCallback* errorCallback, const Dictionary& mediaConstraints, ExceptionState& exceptionState)
{
    if (errorCallback)
        UseCounter::count(context, UseCounter::RTCPeerConnectionCreateAnswerLegacyFailureCallback);
    else
        UseCounter::countDeprecation(context, UseCounter::RTCPeerConnectionCreateAnswerLegacyNoFailureCallback);

    if (mediaConstraints.isObject())
        UseCounter::count(context, UseCounter::RTCPeerConnectionCreateAnswerLegacyConstraints);
    else if (errorCallback)
        UseCounter::count(context, UseCounter::RTCPeerConnectionCreateAnswerLegacyCompliant);

    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    ASSERT(successCallback);

    MediaErrorState mediaErrorState;
    WebMediaConstraints constraints = MediaConstraintsImpl::create(context, mediaConstraints, mediaErrorState);
    if (mediaErrorState.hadException()) {
        mediaErrorState.raiseException(exceptionState);
        return;
    }

    RTCSessionDescriptionRequest* request = RTCSessionDescriptionRequestImpl::create(executionContext(), this, successCallback, errorCallback);
    m_peerHandler->createAnswer(request, constraints);
}

void RTCPeerConnection::setLocalDescription(ExecutionContext* context, RTCSessionDescription* sessionDescription, VoidCallback* successCallback, RTCErrorCallback* errorCallback, ExceptionState& exceptionState)
{
    if (successCallback && errorCallback) {
        UseCounter::count(context, UseCounter::RTCPeerConnectionSetLocalDescriptionLegacyCompliant);
    } else {
        if (!successCallback)
            UseCounter::count(context, UseCounter::RTCPeerConnectionSetLocalDescriptionLegacyNoSuccessCallback);
        if (!errorCallback)
            UseCounter::count(context, UseCounter::RTCPeerConnectionSetLocalDescriptionLegacyNoFailureCallback);
    }

    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    ASSERT(sessionDescription);

    RTCVoidRequest* request = RTCVoidRequestImpl::create(executionContext(), this, successCallback, errorCallback);
    m_peerHandler->setLocalDescription(request, sessionDescription->webSessionDescription());
}

RTCSessionDescription* RTCPeerConnection::localDescription()
{
    WebRTCSessionDescription webSessionDescription = m_peerHandler->localDescription();
    if (webSessionDescription.isNull())
        return nullptr;

    return RTCSessionDescription::create(webSessionDescription);
}

void RTCPeerConnection::setRemoteDescription(ExecutionContext* context, RTCSessionDescription* sessionDescription, VoidCallback* successCallback, RTCErrorCallback* errorCallback, ExceptionState& exceptionState)
{
    if (successCallback && errorCallback) {
        UseCounter::count(context, UseCounter::RTCPeerConnectionSetRemoteDescriptionLegacyCompliant);
    } else {
        if (!successCallback)
            UseCounter::count(context, UseCounter::RTCPeerConnectionSetRemoteDescriptionLegacyNoSuccessCallback);
        if (!errorCallback)
            UseCounter::count(context, UseCounter::RTCPeerConnectionSetRemoteDescriptionLegacyNoFailureCallback);
    }

    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    ASSERT(sessionDescription);

    RTCVoidRequest* request = RTCVoidRequestImpl::create(executionContext(), this, successCallback, errorCallback);
    m_peerHandler->setRemoteDescription(request, sessionDescription->webSessionDescription());
}

RTCSessionDescription* RTCPeerConnection::remoteDescription()
{
    WebRTCSessionDescription webSessionDescription = m_peerHandler->remoteDescription();
    if (webSessionDescription.isNull())
        return nullptr;

    return RTCSessionDescription::create(webSessionDescription);
}

void RTCPeerConnection::updateIce(ExecutionContext* context, const Dictionary& rtcConfiguration, const Dictionary& mediaConstraints, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    RTCConfiguration* configuration = parseConfiguration(rtcConfiguration, exceptionState);
    if (exceptionState.hadException())
        return;

    MediaErrorState mediaErrorState;
    WebMediaConstraints constraints = MediaConstraintsImpl::create(context, mediaConstraints, mediaErrorState);
    if (mediaErrorState.hadException()) {
        mediaErrorState.raiseException(exceptionState);
        return;
    }

    bool valid = m_peerHandler->updateICE(configuration, constraints);
    if (!valid)
        exceptionState.throwDOMException(SyntaxError, "Could not update the ICE Agent with the given configuration.");
}

ScriptPromise RTCPeerConnection::generateCertificate(ScriptState* scriptState, const AlgorithmIdentifier& keygenAlgorithm, ExceptionState& exceptionState)
{
    // Normalize |keygenAlgorithm| with WebCrypto, making sure it is a recognized AlgorithmIdentifier.
    WebCryptoAlgorithm cryptoAlgorithm;
    AlgorithmError error;
    if (!normalizeAlgorithm(keygenAlgorithm, WebCryptoOperationGenerateKey, cryptoAlgorithm, &error)) {
        // Reject generateCertificate with the same error as was produced by WebCrypto.
        // |result| is garbage collected, no need to delete.
        CryptoResultImpl* result = CryptoResultImpl::create(scriptState);
        ScriptPromise promise = result->promise();
        result->completeWithError(error.errorType, error.errorDetails);
        return promise;
    }

    // Convert from WebCrypto representation to recognized WebRTCKeyParams. WebRTC supports a small subset of what are valid AlgorithmIdentifiers.
    const char* unsupportedParamsString = "The 1st argument provided is an AlgorithmIdentifier with a supported algorithm name, but the parameters are not supported.";
    Nullable<WebRTCKeyParams> keyParams;
    switch (cryptoAlgorithm.id()) {
    case WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
        // name: "RSASSA-PKCS1-v1_5"
        unsigned publicExponent;
        // "publicExponent" must fit in an unsigned int. The only recognized "hash" is "SHA-256".
        if (bigIntegerToUint(cryptoAlgorithm.rsaHashedKeyGenParams()->publicExponent(), publicExponent)
            && cryptoAlgorithm.rsaHashedKeyGenParams()->hash().id() == WebCryptoAlgorithmIdSha256) {
            unsigned modulusLength = cryptoAlgorithm.rsaHashedKeyGenParams()->modulusLengthBits();
            keyParams.set(blink::WebRTCKeyParams::createRSA(modulusLength, publicExponent));
        } else {
            return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, unsupportedParamsString));
        }
        break;
    case WebCryptoAlgorithmIdEcdsa:
        // name: "ECDSA"
        // The only recognized "namedCurve" is "P-256".
        if (cryptoAlgorithm.ecKeyGenParams()->namedCurve() == WebCryptoNamedCurveP256) {
            keyParams.set(blink::WebRTCKeyParams::createECDSA(blink::WebRTCECCurveNistP256));
        } else {
            return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, unsupportedParamsString));
        }
        break;
    default:
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "The 1st argument provided is an AlgorithmIdentifier, but the algorithm is not supported."));
        break;
    }
    ASSERT(!keyParams.isNull());

    OwnPtr<WebRTCCertificateGenerator> certificateGenerator = adoptPtr(
        Platform::current()->createRTCCertificateGenerator());

    // |keyParams| was successfully constructed, but does the certificate generator support these parameters?
    if (!certificateGenerator->isSupportedKeyParams(keyParams.get())) {
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, unsupportedParamsString));
    }

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebRTCCertificateObserver* certificateObserver = WebRTCCertificateObserver::create(resolver);

    // Generate certificate. The |certificateObserver| will resolve the promise asynchronously upon completion.
    // The observer will manage its own destruction as well as the resolver's destruction.
    certificateGenerator->generateCertificate(
        keyParams.get(),
        toDocument(scriptState->executionContext())->url(),
        toDocument(scriptState->executionContext())->firstPartyForCookies(),
        certificateObserver);

    return promise;
}

void RTCPeerConnection::addIceCandidate(RTCIceCandidate* iceCandidate, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    ASSERT(iceCandidate);

    bool valid = m_peerHandler->addICECandidate(iceCandidate->webCandidate());
    if (!valid)
        exceptionState.throwDOMException(SyntaxError, "The ICE candidate could not be added.");
}

void RTCPeerConnection::addIceCandidate(RTCIceCandidate* iceCandidate, VoidCallback* successCallback, RTCErrorCallback* errorCallback, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    ASSERT(iceCandidate);
    ASSERT(successCallback);
    ASSERT(errorCallback);

    RTCVoidRequest* request = RTCVoidRequestImpl::create(executionContext(), this, successCallback, errorCallback);

    bool implemented = m_peerHandler->addICECandidate(request, iceCandidate->webCandidate());
    if (!implemented) {
        exceptionState.throwDOMException(NotSupportedError, "This method is not yet implemented.");
    }
}

String RTCPeerConnection::signalingState() const
{
    switch (m_signalingState) {
    case SignalingStateStable:
        return "stable";
    case SignalingStateHaveLocalOffer:
        return "have-local-offer";
    case SignalingStateHaveRemoteOffer:
        return "have-remote-offer";
    case SignalingStateHaveLocalPrAnswer:
        return "have-local-pranswer";
    case SignalingStateHaveRemotePrAnswer:
        return "have-remote-pranswer";
    case SignalingStateClosed:
        return "closed";
    }

    ASSERT_NOT_REACHED();
    return String();
}

String RTCPeerConnection::iceGatheringState() const
{
    switch (m_iceGatheringState) {
    case ICEGatheringStateNew:
        return "new";
    case ICEGatheringStateGathering:
        return "gathering";
    case ICEGatheringStateComplete:
        return "complete";
    }

    ASSERT_NOT_REACHED();
    return String();
}

String RTCPeerConnection::iceConnectionState() const
{
    switch (m_iceConnectionState) {
    case ICEConnectionStateNew:
        return "new";
    case ICEConnectionStateChecking:
        return "checking";
    case ICEConnectionStateConnected:
        return "connected";
    case ICEConnectionStateCompleted:
        return "completed";
    case ICEConnectionStateFailed:
        return "failed";
    case ICEConnectionStateDisconnected:
        return "disconnected";
    case ICEConnectionStateClosed:
        return "closed";
    }

    ASSERT_NOT_REACHED();
    return String();
}

void RTCPeerConnection::addStream(ExecutionContext* context, MediaStream* stream, const Dictionary& mediaConstraints, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    if (!stream) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "MediaStream"));
        return;
    }

    if (m_localStreams.contains(stream))
        return;

    MediaErrorState mediaErrorState;
    WebMediaConstraints constraints = MediaConstraintsImpl::create(context, mediaConstraints, mediaErrorState);
    if (mediaErrorState.hadException()) {
        mediaErrorState.raiseException(exceptionState);
        return;
    }

    m_localStreams.append(stream);

    bool valid = m_peerHandler->addStream(stream->descriptor(), constraints);
    if (!valid)
        exceptionState.throwDOMException(SyntaxError, "Unable to add the provided stream.");
}

void RTCPeerConnection::removeStream(MediaStream* stream, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    if (!stream) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "MediaStream"));
        return;
    }

    size_t pos = m_localStreams.find(stream);
    if (pos == kNotFound)
        return;

    m_localStreams.remove(pos);

    m_peerHandler->removeStream(stream->descriptor());
}

MediaStreamVector RTCPeerConnection::getLocalStreams() const
{
    return m_localStreams;
}

MediaStreamVector RTCPeerConnection::getRemoteStreams() const
{
    return m_remoteStreams;
}

MediaStream* RTCPeerConnection::getStreamById(const String& streamId)
{
    for (MediaStreamVector::iterator iter = m_localStreams.begin(); iter != m_localStreams.end(); ++iter) {
        if ((*iter)->id() == streamId)
            return iter->get();
    }

    for (MediaStreamVector::iterator iter = m_remoteStreams.begin(); iter != m_remoteStreams.end(); ++iter) {
        if ((*iter)->id() == streamId)
            return iter->get();
    }

    return 0;
}

void RTCPeerConnection::getStats(ExecutionContext* context, RTCStatsCallback* successCallback, MediaStreamTrack* selector)
{
    UseCounter::count(context, UseCounter::RTCPeerConnectionGetStatsLegacyNonCompliant);
    RTCStatsRequest* statsRequest = RTCStatsRequestImpl::create(executionContext(), this, successCallback, selector);
    // FIXME: Add passing selector as part of the statsRequest.
    m_peerHandler->getStats(statsRequest);
}

RTCDataChannel* RTCPeerConnection::createDataChannel(String label, const Dictionary& options, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return nullptr;

    WebRTCDataChannelInit init;
    DictionaryHelper::get(options, "ordered", init.ordered);
    DictionaryHelper::get(options, "negotiated", init.negotiated);

    unsigned short value = 0;
    if (DictionaryHelper::get(options, "id", value))
        init.id = value;
    if (DictionaryHelper::get(options, "maxRetransmits", value))
        init.maxRetransmits = value;
    if (DictionaryHelper::get(options, "maxRetransmitTime", value))
        init.maxRetransmitTime = value;

    String protocolString;
    DictionaryHelper::get(options, "protocol", protocolString);
    init.protocol = protocolString;

    RTCDataChannel* channel = RTCDataChannel::create(executionContext(), m_peerHandler.get(), label, init, exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    RTCDataChannel::ReadyState handlerState = channel->getHandlerState();
    if (handlerState != RTCDataChannel::ReadyStateConnecting) {
        // There was an early state transition.  Don't miss it!
        channel->didChangeReadyState(handlerState);
    }
    return channel;
}

bool RTCPeerConnection::hasLocalStreamWithTrackId(const String& trackId)
{
    for (MediaStreamVector::iterator iter = m_localStreams.begin(); iter != m_localStreams.end(); ++iter) {
        if ((*iter)->getTrackById(trackId))
            return true;
    }
    return false;
}

RTCDTMFSender* RTCPeerConnection::createDTMFSender(MediaStreamTrack* track, ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return nullptr;

    ASSERT(track);

    if (!hasLocalStreamWithTrackId(track->id())) {
        exceptionState.throwDOMException(SyntaxError, "No local stream is available for the track provided.");
        return nullptr;
    }

    RTCDTMFSender* dtmfSender = RTCDTMFSender::create(executionContext(), m_peerHandler.get(), track, exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    return dtmfSender;
}

void RTCPeerConnection::close(ExceptionState& exceptionState)
{
    if (throwExceptionIfSignalingStateClosed(m_signalingState, exceptionState))
        return;

    closeInternal();
}

void RTCPeerConnection::negotiationNeeded()
{
    ASSERT(!m_closed);
    scheduleDispatchEvent(Event::create(EventTypeNames::negotiationneeded));
}

void RTCPeerConnection::didGenerateICECandidate(const WebRTCICECandidate& webCandidate)
{
    ASSERT(!m_closed);
    ASSERT(executionContext()->isContextThread());
    if (webCandidate.isNull())
        scheduleDispatchEvent(RTCIceCandidateEvent::create(false, false, nullptr));
    else {
        RTCIceCandidate* iceCandidate = RTCIceCandidate::create(webCandidate);
        scheduleDispatchEvent(RTCIceCandidateEvent::create(false, false, iceCandidate));
    }
}

void RTCPeerConnection::didChangeSignalingState(SignalingState newState)
{
    ASSERT(!m_closed);
    ASSERT(executionContext()->isContextThread());
    changeSignalingState(newState);
}

void RTCPeerConnection::didChangeICEGatheringState(ICEGatheringState newState)
{
    ASSERT(!m_closed);
    ASSERT(executionContext()->isContextThread());
    changeIceGatheringState(newState);
}

void RTCPeerConnection::didChangeICEConnectionState(ICEConnectionState newState)
{
    ASSERT(!m_closed);
    ASSERT(executionContext()->isContextThread());
    changeIceConnectionState(newState);
}

void RTCPeerConnection::didAddRemoteStream(const WebMediaStream& remoteStream)
{
    ASSERT(!m_closed);
    ASSERT(executionContext()->isContextThread());

    if (m_signalingState == SignalingStateClosed)
        return;

    MediaStream* stream = MediaStream::create(executionContext(), remoteStream);
    m_remoteStreams.append(stream);

    scheduleDispatchEvent(MediaStreamEvent::create(EventTypeNames::addstream, false, false, stream));
}

void RTCPeerConnection::didRemoveRemoteStream(const WebMediaStream& remoteStream)
{
    ASSERT(!m_closed);
    ASSERT(executionContext()->isContextThread());

    MediaStreamDescriptor* streamDescriptor = remoteStream;
    ASSERT(streamDescriptor->client());

    MediaStream* stream = static_cast<MediaStream*>(streamDescriptor->client());
    stream->streamEnded();

    if (m_signalingState == SignalingStateClosed)
        return;

    size_t pos = m_remoteStreams.find(stream);
    ASSERT(pos != kNotFound);
    m_remoteStreams.remove(pos);

    scheduleDispatchEvent(MediaStreamEvent::create(EventTypeNames::removestream, false, false, stream));
}

void RTCPeerConnection::didAddRemoteDataChannel(WebRTCDataChannelHandler* handler)
{
    ASSERT(!m_closed);
    ASSERT(executionContext()->isContextThread());

    if (m_signalingState == SignalingStateClosed)
        return;

    RTCDataChannel* channel = RTCDataChannel::create(executionContext(), adoptPtr(handler));
    scheduleDispatchEvent(RTCDataChannelEvent::create(EventTypeNames::datachannel, false, false, channel));
}

void RTCPeerConnection::releasePeerConnectionHandler()
{
    stop();
}

void RTCPeerConnection::closePeerConnection()
{
    ASSERT(m_signalingState != RTCPeerConnection::SignalingStateClosed);
    closeInternal();
}

const AtomicString& RTCPeerConnection::interfaceName() const
{
    return EventTargetNames::RTCPeerConnection;
}

ExecutionContext* RTCPeerConnection::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

void RTCPeerConnection::suspend()
{
    m_dispatchScheduledEventRunner->suspend();
}

void RTCPeerConnection::resume()
{
    m_dispatchScheduledEventRunner->resume();
}

void RTCPeerConnection::stop()
{
    if (m_stopped)
        return;

    m_stopped = true;
    m_iceConnectionState = ICEConnectionStateClosed;
    m_signalingState = SignalingStateClosed;

    m_dispatchScheduledEventRunner->stop();

    m_peerHandler.clear();
}

void RTCPeerConnection::changeSignalingState(SignalingState signalingState)
{
    if (m_signalingState != SignalingStateClosed && m_signalingState != signalingState) {
        m_signalingState = signalingState;
        scheduleDispatchEvent(Event::create(EventTypeNames::signalingstatechange));
    }
}

void RTCPeerConnection::changeIceGatheringState(ICEGatheringState iceGatheringState)
{
    m_iceGatheringState = iceGatheringState;
}

bool RTCPeerConnection::setIceConnectionState(ICEConnectionState iceConnectionState)
{
    if (m_iceConnectionState != ICEConnectionStateClosed && m_iceConnectionState != iceConnectionState) {
        m_iceConnectionState = iceConnectionState;
        return true;
    }
    return false;
}

void RTCPeerConnection::changeIceConnectionState(ICEConnectionState iceConnectionState)
{
    if (m_iceConnectionState != ICEConnectionStateClosed) {
        scheduleDispatchEvent(Event::create(EventTypeNames::iceconnectionstatechange),
            WTF::bind(&RTCPeerConnection::setIceConnectionState, this, iceConnectionState));
    }
}

void RTCPeerConnection::closeInternal()
{
    ASSERT(m_signalingState != RTCPeerConnection::SignalingStateClosed);
    m_peerHandler->stop();
    m_closed = true;

    changeIceConnectionState(ICEConnectionStateClosed);
    changeIceGatheringState(ICEGatheringStateComplete);
    changeSignalingState(SignalingStateClosed);
}

void RTCPeerConnection::scheduleDispatchEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    scheduleDispatchEvent(event, nullptr);
}

void RTCPeerConnection::scheduleDispatchEvent(PassRefPtrWillBeRawPtr<Event> event,
    PassOwnPtr<BoolFunction> setupFunction)
{
    m_scheduledEvents.append(new EventWrapper(event, setupFunction));

    m_dispatchScheduledEventRunner->runAsync();
}

void RTCPeerConnection::dispatchScheduledEvent()
{
    if (m_stopped)
        return;

    HeapVector<Member<EventWrapper>> events;
    events.swap(m_scheduledEvents);

    HeapVector<Member<EventWrapper>>::iterator it = events.begin();
    for (; it != events.end(); ++it) {
        if ((*it)->setup()) {
            dispatchEvent((*it)->m_event.release());
        }
    }

    events.clear();
}

DEFINE_TRACE(RTCPeerConnection)
{
    visitor->trace(m_localStreams);
    visitor->trace(m_remoteStreams);
    visitor->trace(m_dispatchScheduledEventRunner);
    visitor->trace(m_scheduledEvents);
    RefCountedGarbageCollectedEventTargetWithInlineData<RTCPeerConnection>::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
