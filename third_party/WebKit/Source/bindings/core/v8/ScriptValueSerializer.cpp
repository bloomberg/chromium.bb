// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/ScriptValueSerializer.h"

#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8ArrayBufferView.h"
#include "bindings/core/v8/V8Blob.h"
#include "bindings/core/v8/V8File.h"
#include "bindings/core/v8/V8FileList.h"
#include "bindings/core/v8/V8ImageData.h"
#include "bindings/core/v8/V8MessagePort.h"
#include "bindings/modules/v8/V8CryptoKey.h"
#include "bindings/modules/v8/V8DOMFileSystem.h"
#include "core/dom/DOMDataView.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/File.h"
#include "core/fileapi/FileList.h"
#include "public/platform/Platform.h"
#include "public/platform/WebBlobInfo.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/ArrayBufferContents.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/StringUTF8Adaptor.h"

// FIXME: consider crashing in debug mode on deserialization errors
// NOTE: be sure to change wireFormatVersion as necessary!

namespace blink {

namespace SerializedScriptValueInternal {

// This code implements the HTML5 Structured Clone algorithm:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/urls.html#safe-passing-of-structured-data

// ZigZag encoding helps VarInt encoding stay small for negative
// numbers with small absolute values.
class ZigZag {
public:
    static uint32_t encode(uint32_t value)
    {
        if (value & (1U << 31))
            value = ((~value) << 1) + 1;
        else
            value <<= 1;
        return value;
    }

    static uint32_t decode(uint32_t value)
    {
        if (value & 1)
            value = ~(value >> 1);
        else
            value >>= 1;
        return value;
    }

private:
    ZigZag();
};

static const int maxDepth = 20000;

static bool shouldCheckForCycles(int depth)
{
    ASSERT(depth >= 0);
    // Since we are not required to spot the cycle as soon as it
    // happens we can check for cycles only when the current depth
    // is a power of two.
    return !(depth & (depth - 1));
}

void Writer::writeUndefined()
{
    append(UndefinedTag);
}

void Writer::writeNull()
{
    append(NullTag);
}

void Writer::writeTrue()
{
    append(TrueTag);
}

void Writer::writeFalse()
{
    append(FalseTag);
}

void Writer::writeBooleanObject(bool value)
{
    append(value ? TrueObjectTag : FalseObjectTag);
}

void Writer::writeOneByteString(v8::Handle<v8::String>& string)
{
    int stringLength = string->Length();
    int utf8Length = string->Utf8Length();
    ASSERT(stringLength >= 0 && utf8Length >= 0);

    append(StringTag);
    doWriteUint32(static_cast<uint32_t>(utf8Length));
    ensureSpace(utf8Length);

    // ASCII fast path.
    if (stringLength == utf8Length) {
        string->WriteOneByte(byteAt(m_position), 0, utf8Length, v8StringWriteOptions());
    } else {
        char* buffer = reinterpret_cast<char*>(byteAt(m_position));
        string->WriteUtf8(buffer, utf8Length, 0, v8StringWriteOptions());
    }
    m_position += utf8Length;
}

void Writer::writeUCharString(v8::Handle<v8::String>& string)
{
    int length = string->Length();
    ASSERT(length >= 0);

    int size = length * sizeof(UChar);
    int bytes = bytesNeededToWireEncode(static_cast<uint32_t>(size));
    if ((m_position + 1 + bytes) & 1)
        append(PaddingTag);

    append(StringUCharTag);
    doWriteUint32(static_cast<uint32_t>(size));
    ensureSpace(size);

    ASSERT(!(m_position & 1));
    uint16_t* buffer = reinterpret_cast<uint16_t*>(byteAt(m_position));
    string->Write(buffer, 0, length, v8StringWriteOptions());
    m_position += size;
}

void Writer::writeStringObject(const char* data, int length)
{
    ASSERT(length >= 0);
    append(StringObjectTag);
    doWriteString(data, length);
}

void Writer::writeWebCoreString(const String& string)
{
    // Uses UTF8 encoding so we can read it back as either V8 or
    // WebCore string.
    append(StringTag);
    doWriteWebCoreString(string);
}

void Writer::writeVersion()
{
    append(VersionTag);
    doWriteUint32(SerializedScriptValue::wireFormatVersion);
}

void Writer::writeInt32(int32_t value)
{
    append(Int32Tag);
    doWriteUint32(ZigZag::encode(static_cast<uint32_t>(value)));
}

void Writer::writeUint32(uint32_t value)
{
    append(Uint32Tag);
    doWriteUint32(value);
}

void Writer::writeDate(double numberValue)
{
    append(DateTag);
    doWriteNumber(numberValue);
}

void Writer::writeNumber(double number)
{
    append(NumberTag);
    doWriteNumber(number);
}

void Writer::writeNumberObject(double number)
{
    append(NumberObjectTag);
    doWriteNumber(number);
}

void Writer::writeBlob(const String& uuid, const String& type, unsigned long long size)
{
    append(BlobTag);
    doWriteWebCoreString(uuid);
    doWriteWebCoreString(type);
    doWriteUint64(size);
}

void Writer::writeDOMFileSystem(int type, const String& name, const String& url)
{
    append(DOMFileSystemTag);
    doWriteUint32(type);
    doWriteWebCoreString(name);
    doWriteWebCoreString(url);
}

void Writer::writeBlobIndex(int blobIndex)
{
    ASSERT(blobIndex >= 0);
    append(BlobIndexTag);
    doWriteUint32(blobIndex);
}

void Writer::writeFile(const File& file)
{
    append(FileTag);
    doWriteFile(file);
}

void Writer::writeFileIndex(int blobIndex)
{
    append(FileIndexTag);
    doWriteUint32(blobIndex);
}

void Writer::writeFileList(const FileList& fileList)
{
    append(FileListTag);
    uint32_t length = fileList.length();
    doWriteUint32(length);
    for (unsigned i = 0; i < length; ++i)
        doWriteFile(*fileList.item(i));
}

void Writer::writeFileListIndex(const Vector<int>& blobIndices)
{
    append(FileListIndexTag);
    uint32_t length = blobIndices.size();
    doWriteUint32(length);
    for (unsigned i = 0; i < length; ++i)
        doWriteUint32(blobIndices[i]);
}

bool Writer::writeCryptoKey(const WebCryptoKey& key)
{
    append(static_cast<uint8_t>(CryptoKeyTag));

    switch (key.algorithm().paramsType()) {
    case WebCryptoKeyAlgorithmParamsTypeAes:
        doWriteAesKey(key);
        break;
    case WebCryptoKeyAlgorithmParamsTypeHmac:
        doWriteHmacKey(key);
        break;
    case WebCryptoKeyAlgorithmParamsTypeRsaHashed:
        doWriteRsaHashedKey(key);
        break;
    case WebCryptoKeyAlgorithmParamsTypeEc:
        doWriteEcKey(key);
        break;
    case WebCryptoKeyAlgorithmParamsTypeNone:
        ASSERT_NOT_REACHED();
        return false;
    }

    doWriteKeyUsages(key.usages(), key.extractable());

    WebVector<uint8_t> keyData;
    if (!Platform::current()->crypto()->serializeKeyForClone(key, keyData))
        return false;

    doWriteUint32(keyData.size());
    append(keyData.data(), keyData.size());
    return true;
}

void Writer::writeArrayBuffer(const ArrayBuffer& arrayBuffer)
{
    append(ArrayBufferTag);
    doWriteArrayBuffer(arrayBuffer);
}

void Writer::writeArrayBufferView(const ArrayBufferView& arrayBufferView)
{
    append(ArrayBufferViewTag);
#if ENABLE(ASSERT)
    const ArrayBuffer& arrayBuffer = *arrayBufferView.buffer();
    ASSERT(static_cast<const uint8_t*>(arrayBuffer.data()) + arrayBufferView.byteOffset() ==
        static_cast<const uint8_t*>(arrayBufferView.baseAddress()));
#endif
    ArrayBufferView::ViewType type = arrayBufferView.type();

    if (type == ArrayBufferView::TypeInt8)
        append(ByteArrayTag);
    else if (type == ArrayBufferView::TypeUint8Clamped)
        append(UnsignedByteClampedArrayTag);
    else if (type == ArrayBufferView::TypeUint8)
        append(UnsignedByteArrayTag);
    else if (type == ArrayBufferView::TypeInt16)
        append(ShortArrayTag);
    else if (type == ArrayBufferView::TypeUint16)
        append(UnsignedShortArrayTag);
    else if (type == ArrayBufferView::TypeInt32)
        append(IntArrayTag);
    else if (type == ArrayBufferView::TypeUint32)
        append(UnsignedIntArrayTag);
    else if (type == ArrayBufferView::TypeFloat32)
        append(FloatArrayTag);
    else if (type == ArrayBufferView::TypeFloat64)
        append(DoubleArrayTag);
    else if (type == ArrayBufferView::TypeDataView)
        append(DataViewTag);
    else
        ASSERT_NOT_REACHED();
    doWriteUint32(arrayBufferView.byteOffset());
    doWriteUint32(arrayBufferView.byteLength());
}

void Writer::writeImageData(uint32_t width, uint32_t height, const uint8_t* pixelData, uint32_t pixelDataLength)
{
    append(ImageDataTag);
    doWriteUint32(width);
    doWriteUint32(height);
    doWriteUint32(pixelDataLength);
    append(pixelData, pixelDataLength);
}

void Writer::writeRegExp(v8::Local<v8::String> pattern, v8::RegExp::Flags flags)
{
    append(RegExpTag);
    v8::String::Utf8Value patternUtf8Value(pattern);
    doWriteString(*patternUtf8Value, patternUtf8Value.length());
    doWriteUint32(static_cast<uint32_t>(flags));
}

void Writer::writeTransferredMessagePort(uint32_t index)
{
    append(MessagePortTag);
    doWriteUint32(index);
}

void Writer::writeTransferredArrayBuffer(uint32_t index)
{
    append(ArrayBufferTransferTag);
    doWriteUint32(index);
}

void Writer::writeObjectReference(uint32_t reference)
{
    append(ObjectReferenceTag);
    doWriteUint32(reference);
}

void Writer::writeObject(uint32_t numProperties)
{
    append(ObjectTag);
    doWriteUint32(numProperties);
}

void Writer::writeSparseArray(uint32_t numProperties, uint32_t length)
{
    append(SparseArrayTag);
    doWriteUint32(numProperties);
    doWriteUint32(length);
}

void Writer::writeDenseArray(uint32_t numProperties, uint32_t length)
{
    append(DenseArrayTag);
    doWriteUint32(numProperties);
    doWriteUint32(length);
}

String Writer::takeWireString()
{
    COMPILE_ASSERT(sizeof(BufferValueType) == 2, BufferValueTypeIsTwoBytes);
    fillHole();
    String data = String(m_buffer.data(), m_buffer.size());
    data.impl()->truncateAssumingIsolated((m_position + 1) / sizeof(BufferValueType));
    return data;
}

void Writer::writeReferenceCount(uint32_t numberOfReferences)
{
    append(ReferenceCountTag);
    doWriteUint32(numberOfReferences);
}

void Writer::writeGenerateFreshObject()
{
    append(GenerateFreshObjectTag);
}

void Writer::writeGenerateFreshSparseArray(uint32_t length)
{
    append(GenerateFreshSparseArrayTag);
    doWriteUint32(length);
}

void Writer::writeGenerateFreshDenseArray(uint32_t length)
{
    append(GenerateFreshDenseArrayTag);
    doWriteUint32(length);
}

void Writer::doWriteFile(const File& file)
{
    doWriteWebCoreString(file.hasBackingFile() ? file.path() : "");
    doWriteWebCoreString(file.name());
    doWriteWebCoreString(file.webkitRelativePath());
    doWriteWebCoreString(file.uuid());
    doWriteWebCoreString(file.type());

    // FIXME don't use 1 byte to encode a flag.
    if (file.hasValidSnapshotMetadata()) {
        doWriteUint32(static_cast<uint8_t>(1));

        long long size;
        double lastModified;
        file.captureSnapshot(size, lastModified);
        doWriteUint64(static_cast<uint64_t>(size));
        doWriteNumber(lastModified);
    } else {
        doWriteUint32(static_cast<uint8_t>(0));
    }

    doWriteUint32(static_cast<uint8_t>((file.userVisibility() == File::IsUserVisible) ? 1 : 0));
}

void Writer::doWriteArrayBuffer(const ArrayBuffer& arrayBuffer)
{
    uint32_t byteLength = arrayBuffer.byteLength();
    doWriteUint32(byteLength);
    append(static_cast<const uint8_t*>(arrayBuffer.data()), byteLength);
}

void Writer::doWriteString(const char* data, int length)
{
    doWriteUint32(static_cast<uint32_t>(length));
    append(reinterpret_cast<const uint8_t*>(data), length);
}

void Writer::doWriteWebCoreString(const String& string)
{
    StringUTF8Adaptor stringUTF8(string);
    doWriteString(stringUTF8.data(), stringUTF8.length());
}

void Writer::doWriteHmacKey(const WebCryptoKey& key)
{
    ASSERT(key.algorithm().paramsType() == WebCryptoKeyAlgorithmParamsTypeHmac);

    append(static_cast<uint8_t>(HmacKeyTag));
    ASSERT(!(key.algorithm().hmacParams()->lengthBits() % 8));
    doWriteUint32(key.algorithm().hmacParams()->lengthBits() / 8);
    doWriteAlgorithmId(key.algorithm().hmacParams()->hash().id());
}

void Writer::doWriteAesKey(const WebCryptoKey& key)
{
    ASSERT(key.algorithm().paramsType() == WebCryptoKeyAlgorithmParamsTypeAes);

    append(static_cast<uint8_t>(AesKeyTag));
    doWriteAlgorithmId(key.algorithm().id());
    // Converting the key length from bits to bytes is lossless and makes
    // it fit in 1 byte.
    ASSERT(!(key.algorithm().aesParams()->lengthBits() % 8));
    doWriteUint32(key.algorithm().aesParams()->lengthBits() / 8);
}

void Writer::doWriteRsaHashedKey(const WebCryptoKey& key)
{
    ASSERT(key.algorithm().rsaHashedParams());
    append(static_cast<uint8_t>(RsaHashedKeyTag));

    doWriteAlgorithmId(key.algorithm().id());
    doWriteAsymmetricKeyType(key.type());

    const WebCryptoRsaHashedKeyAlgorithmParams* params = key.algorithm().rsaHashedParams();
    doWriteUint32(params->modulusLengthBits());
    doWriteUint32(params->publicExponent().size());
    append(params->publicExponent().data(), params->publicExponent().size());
    doWriteAlgorithmId(params->hash().id());
}

void Writer::doWriteEcKey(const WebCryptoKey& key)
{
    ASSERT(key.algorithm().ecParams());
    append(static_cast<uint8_t>(EcKeyTag));

    doWriteAlgorithmId(key.algorithm().id());
    doWriteAsymmetricKeyType(key.type());
    doWriteNamedCurve(key.algorithm().ecParams()->namedCurve());
}

void Writer::doWriteAlgorithmId(WebCryptoAlgorithmId id)
{
    switch (id) {
    case WebCryptoAlgorithmIdAesCbc:
        return doWriteUint32(AesCbcTag);
    case WebCryptoAlgorithmIdHmac:
        return doWriteUint32(HmacTag);
    case WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
        return doWriteUint32(RsaSsaPkcs1v1_5Tag);
    case WebCryptoAlgorithmIdSha1:
        return doWriteUint32(Sha1Tag);
    case WebCryptoAlgorithmIdSha256:
        return doWriteUint32(Sha256Tag);
    case WebCryptoAlgorithmIdSha384:
        return doWriteUint32(Sha384Tag);
    case WebCryptoAlgorithmIdSha512:
        return doWriteUint32(Sha512Tag);
    case WebCryptoAlgorithmIdAesGcm:
        return doWriteUint32(AesGcmTag);
    case WebCryptoAlgorithmIdRsaOaep:
        return doWriteUint32(RsaOaepTag);
    case WebCryptoAlgorithmIdAesCtr:
        return doWriteUint32(AesCtrTag);
    case WebCryptoAlgorithmIdAesKw:
        return doWriteUint32(AesKwTag);
    case WebCryptoAlgorithmIdRsaPss:
        return doWriteUint32(RsaPssTag);
    case WebCryptoAlgorithmIdEcdsa:
        return doWriteUint32(EcdsaTag);
    }
    ASSERT_NOT_REACHED();
}

void Writer::doWriteAsymmetricKeyType(WebCryptoKeyType keyType)
{
    switch (keyType) {
    case WebCryptoKeyTypePublic:
        doWriteUint32(PublicKeyType);
        break;
    case WebCryptoKeyTypePrivate:
        doWriteUint32(PrivateKeyType);
        break;
    case WebCryptoKeyTypeSecret:
        ASSERT_NOT_REACHED();
    }
}

void Writer::doWriteNamedCurve(WebCryptoNamedCurve namedCurve)
{
    switch (namedCurve) {
    case WebCryptoNamedCurveP256:
        return doWriteUint32(P256Tag);
    case WebCryptoNamedCurveP384:
        return doWriteUint32(P384Tag);
    case WebCryptoNamedCurveP521:
        return doWriteUint32(P521Tag);
    }
    ASSERT_NOT_REACHED();
}

void Writer::doWriteKeyUsages(const WebCryptoKeyUsageMask usages, bool extractable)
{
    // Reminder to update this when adding new key usages.
    COMPILE_ASSERT(EndOfWebCryptoKeyUsage == (1 << 7) + 1, UpdateMe);

    uint32_t value = 0;

    if (extractable)
        value |= ExtractableUsage;

    if (usages & WebCryptoKeyUsageEncrypt)
        value |= EncryptUsage;
    if (usages & WebCryptoKeyUsageDecrypt)
        value |= DecryptUsage;
    if (usages & WebCryptoKeyUsageSign)
        value |= SignUsage;
    if (usages & WebCryptoKeyUsageVerify)
        value |= VerifyUsage;
    if (usages & WebCryptoKeyUsageDeriveKey)
        value |= DeriveKeyUsage;
    if (usages & WebCryptoKeyUsageWrapKey)
        value |= WrapKeyUsage;
    if (usages & WebCryptoKeyUsageUnwrapKey)
        value |= UnwrapKeyUsage;
    if (usages & WebCryptoKeyUsageDeriveBits)
        value |= DeriveBitsUsage;

    doWriteUint32(value);
}

int Writer::bytesNeededToWireEncode(uint32_t value)
{
    int bytes = 1;
    while (true) {
        value >>= SerializedScriptValue::varIntShift;
        if (!value)
            break;
        ++bytes;
    }

    return bytes;
}

void Writer::doWriteUint32(uint32_t value)
{
    doWriteUintHelper(value);
}

void Writer::doWriteUint64(uint64_t value)
{
    doWriteUintHelper(value);
}

void Writer::doWriteNumber(double number)
{
    append(reinterpret_cast<uint8_t*>(&number), sizeof(number));
}

void Writer::append(SerializationTag tag)
{
    append(static_cast<uint8_t>(tag));
}

void Writer::append(uint8_t b)
{
    ensureSpace(1);
    *byteAt(m_position++) = b;
}

void Writer::append(const uint8_t* data, int length)
{
    ensureSpace(length);
    memcpy(byteAt(m_position), data, length);
    m_position += length;
}

void Writer::ensureSpace(unsigned extra)
{
    COMPILE_ASSERT(sizeof(BufferValueType) == 2, BufferValueTypeIsTwoBytes);
    m_buffer.resize((m_position + extra + 1) / sizeof(BufferValueType)); // "+ 1" to round up.
}

void Writer::fillHole()
{
    COMPILE_ASSERT(sizeof(BufferValueType) == 2, BufferValueTypeIsTwoBytes);
    // If the writer is at odd position in the buffer, then one of
    // the bytes in the last UChar is not initialized.
    if (m_position % 2)
        *byteAt(m_position) = static_cast<uint8_t>(PaddingTag);
}

uint8_t* Writer::byteAt(int position)
{
    return reinterpret_cast<uint8_t*>(m_buffer.data()) + position;
}

int Writer::v8StringWriteOptions()
{
    return v8::String::NO_NULL_TERMINATION;
}

Serializer::StateBase* Serializer::AbstractObjectState::serializeProperties(bool ignoreIndexed, Serializer& serializer)
{
    while (m_index < m_propertyNames->Length()) {
        if (!m_nameDone) {
            v8::Local<v8::Value> propertyName = m_propertyNames->Get(m_index);
            if (StateBase* newState = serializer.checkException(this))
                return newState;
            if (propertyName.IsEmpty())
                return serializer.handleError(InputError, "Empty property names cannot be cloned.", this);
            bool hasStringProperty = propertyName->IsString() && composite()->HasRealNamedProperty(propertyName.As<v8::String>());
            if (StateBase* newState = serializer.checkException(this))
                return newState;
            bool hasIndexedProperty = !hasStringProperty && propertyName->IsUint32() && composite()->HasRealIndexedProperty(propertyName->Uint32Value());
            if (StateBase* newState = serializer.checkException(this))
                return newState;
            if (hasStringProperty || (hasIndexedProperty && !ignoreIndexed)) {
                m_propertyName = propertyName;
            } else {
                ++m_index;
                continue;
            }
        }
        ASSERT(!m_propertyName.IsEmpty());
        if (!m_nameDone) {
            m_nameDone = true;
            if (StateBase* newState = serializer.doSerialize(m_propertyName, this))
                return newState;
        }
        v8::Local<v8::Value> value = composite()->Get(m_propertyName);
        if (StateBase* newState = serializer.checkException(this))
            return newState;
        m_nameDone = false;
        m_propertyName.Clear();
        ++m_index;
        ++m_numSerializedProperties;
        // If we return early here, it's either because we have pushed a new state onto the
        // serialization state stack or because we have encountered an error (and in both cases
        // we are unwinding the native stack).
        if (StateBase* newState = serializer.doSerialize(value, this))
            return newState;
    }
    return objectDone(m_numSerializedProperties, serializer);
}

Serializer::StateBase* Serializer::ObjectState::advance(Serializer& serializer)
{
    if (m_propertyNames.IsEmpty()) {
        m_propertyNames = composite()->GetPropertyNames();
        if (StateBase* newState = serializer.checkException(this))
            return newState;
        if (m_propertyNames.IsEmpty())
            return serializer.handleError(InputError, "Empty property names cannot be cloned.", nextState());
    }
    return serializeProperties(false, serializer);
}

Serializer::StateBase* Serializer::ObjectState::objectDone(unsigned numProperties, Serializer& serializer)
{
    return serializer.writeObject(numProperties, this);
}

Serializer::StateBase* Serializer::DenseArrayState::advance(Serializer& serializer)
{
    while (m_arrayIndex < m_arrayLength) {
        v8::Handle<v8::Value> value = composite().As<v8::Array>()->Get(m_arrayIndex);
        m_arrayIndex++;
        if (StateBase* newState = serializer.checkException(this))
            return newState;
        if (StateBase* newState = serializer.doSerialize(value, this))
            return newState;
    }
    return serializeProperties(true, serializer);
}

Serializer::StateBase* Serializer::DenseArrayState::objectDone(unsigned numProperties, Serializer& serializer)
{
    return serializer.writeDenseArray(numProperties, m_arrayLength, this);
}

Serializer::StateBase* Serializer::SparseArrayState::advance(Serializer& serializer)
{
    return serializeProperties(false, serializer);
}

Serializer::StateBase* Serializer::SparseArrayState::objectDone(unsigned numProperties, Serializer& serializer)
{
    return serializer.writeSparseArray(numProperties, composite().As<v8::Array>()->Length(), this);
}

static v8::Handle<v8::Object> toV8Object(MessagePort* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (!impl)
        return v8::Handle<v8::Object>();
    v8::Handle<v8::Value> wrapper = toV8(impl, creationContext, isolate);
    ASSERT(wrapper->IsObject());
    return wrapper.As<v8::Object>();
}

static v8::Handle<v8::ArrayBuffer> toV8Object(DOMArrayBuffer* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (!impl)
        return v8::Handle<v8::ArrayBuffer>();
    v8::Handle<v8::Value> wrapper = toV8(impl, creationContext, isolate);
    ASSERT(wrapper->IsArrayBuffer());
    return wrapper.As<v8::ArrayBuffer>();
}

// Returns true if the provided object is to be considered a 'host object', as used in the
// HTML5 structured clone algorithm.
static bool isHostObject(v8::Handle<v8::Object> object)
{
    // If the object has any internal fields, then we won't be able to serialize or deserialize
    // them; conveniently, this is also a quick way to detect DOM wrapper objects, because
    // the mechanism for these relies on data stored in these fields. We should
    // catch external array data as a special case.
    return object->InternalFieldCount() || object->HasIndexedPropertiesInExternalArrayData();
}

Serializer::Serializer(Writer& writer, MessagePortArray* messagePorts, ArrayBufferArray* arrayBuffers, WebBlobInfoArray* blobInfo, BlobDataHandleMap& blobDataHandles, v8::TryCatch& tryCatch, ScriptState* scriptState)
    : m_scriptState(scriptState)
    , m_writer(writer)
    , m_tryCatch(tryCatch)
    , m_depth(0)
    , m_status(Success)
    , m_nextObjectReference(0)
    , m_blobInfo(blobInfo)
    , m_blobDataHandles(blobDataHandles)
{
    ASSERT(!tryCatch.HasCaught());
    v8::Handle<v8::Object> creationContext = m_scriptState->context()->Global();
    if (messagePorts) {
        for (size_t i = 0; i < messagePorts->size(); i++)
            m_transferredMessagePorts.set(toV8Object(messagePorts->at(i).get(), creationContext, isolate()), i);
    }
    if (arrayBuffers) {
        for (size_t i = 0; i < arrayBuffers->size(); i++)  {
            v8::Handle<v8::Object> v8ArrayBuffer = toV8Object(arrayBuffers->at(i).get(), creationContext, isolate());
            // Coalesce multiple occurences of the same buffer to the first index.
            if (!m_transferredArrayBuffers.contains(v8ArrayBuffer))
                m_transferredArrayBuffers.set(v8ArrayBuffer, i);
        }
    }
}

Serializer::Status Serializer::serialize(v8::Handle<v8::Value> value)
{
    v8::HandleScope scope(isolate());
    m_writer.writeVersion();
    StateBase* state = doSerialize(value, 0);
    while (state)
        state = state->advance(*this);
    return m_status;
}

Serializer::StateBase* Serializer::doSerialize(v8::Handle<v8::Value> value, Serializer::StateBase* next)
{
    m_writer.writeReferenceCount(m_nextObjectReference);
    uint32_t objectReference;
    uint32_t arrayBufferIndex;
    if ((value->IsObject() || value->IsDate() || value->IsRegExp())
        && m_objectPool.tryGet(value.As<v8::Object>(), &objectReference)) {
        // Note that IsObject() also detects wrappers (eg, it will catch the things
        // that we grey and write below).
        ASSERT(!value->IsString());
        m_writer.writeObjectReference(objectReference);
    } else if (value.IsEmpty()) {
        return handleError(InputError, "The empty property name cannot be cloned.", next);
    } else if (value->IsUndefined()) {
        m_writer.writeUndefined();
    } else if (value->IsNull()) {
        m_writer.writeNull();
    } else if (value->IsTrue()) {
        m_writer.writeTrue();
    } else if (value->IsFalse()) {
        m_writer.writeFalse();
    } else if (value->IsInt32()) {
        m_writer.writeInt32(value->Int32Value());
    } else if (value->IsUint32()) {
        m_writer.writeUint32(value->Uint32Value());
    } else if (value->IsNumber()) {
        m_writer.writeNumber(value.As<v8::Number>()->Value());
    } else if (V8ArrayBufferView::hasInstance(value, isolate())) {
        return writeAndGreyArrayBufferView(value.As<v8::Object>(), next);
    } else if (value->IsString()) {
        writeString(value);
    } else if (V8MessagePort::hasInstance(value, isolate())) {
        uint32_t messagePortIndex;
        if (m_transferredMessagePorts.tryGet(value.As<v8::Object>(), &messagePortIndex)) {
            m_writer.writeTransferredMessagePort(messagePortIndex);
        } else {
            return handleError(DataCloneError, "A MessagePort could not be cloned.", next);
        }
    } else if (V8ArrayBuffer::hasInstance(value, isolate()) && m_transferredArrayBuffers.tryGet(value.As<v8::Object>(), &arrayBufferIndex)) {
        return writeTransferredArrayBuffer(value, arrayBufferIndex, next);
    } else {
        v8::Handle<v8::Object> jsObject = value.As<v8::Object>();
        if (jsObject.IsEmpty())
            return handleError(DataCloneError, "An object could not be cloned.", next);
        greyObject(jsObject);
        if (value->IsDate()) {
            m_writer.writeDate(value->NumberValue());
        } else if (value->IsStringObject()) {
            writeStringObject(value);
        } else if (value->IsNumberObject()) {
            writeNumberObject(value);
        } else if (value->IsBooleanObject()) {
            writeBooleanObject(value);
        } else if (value->IsArray()) {
            return startArrayState(value.As<v8::Array>(), next);
        } else if (V8File::hasInstance(value, isolate())) {
            return writeFile(value, next);
        } else if (V8Blob::hasInstance(value, isolate())) {
            return writeBlob(value, next);
        } else if (V8DOMFileSystem::hasInstance(value, isolate())) {
            return writeDOMFileSystem(value, next);
        } else if (V8FileList::hasInstance(value, isolate())) {
            return writeFileList(value, next);
        } else if (V8CryptoKey::hasInstance(value, isolate())) {
            if (!writeCryptoKey(value))
                return handleError(DataCloneError, "Couldn't serialize key data", next);
        } else if (V8ImageData::hasInstance(value, isolate())) {
            writeImageData(value);
        } else if (value->IsRegExp()) {
            writeRegExp(value);
        } else if (V8ArrayBuffer::hasInstance(value, isolate())) {
            return writeArrayBuffer(value, next);
        } else if (value->IsObject()) {
            if (isHostObject(jsObject) || jsObject->IsCallable() || value->IsNativeError())
                return handleError(DataCloneError, "An object could not be cloned.", next);
            return startObjectState(jsObject, next);
        } else {
            return handleError(DataCloneError, "A value could not be cloned.", next);
        }
    }
    return 0;
}

Serializer::StateBase* Serializer::doSerializeArrayBuffer(v8::Handle<v8::Value> arrayBuffer, Serializer::StateBase* next)
{
    return doSerialize(arrayBuffer, next);
}

Serializer::StateBase* Serializer::checkException(Serializer::StateBase* state)
{
    return m_tryCatch.HasCaught() ? handleError(JSException, "", state) : 0;
}

Serializer::StateBase* Serializer::writeObject(uint32_t numProperties, Serializer::StateBase* state)
{
    m_writer.writeObject(numProperties);
    return pop(state);
}

Serializer::StateBase* Serializer::writeSparseArray(uint32_t numProperties, uint32_t length, Serializer::StateBase* state)
{
    m_writer.writeSparseArray(numProperties, length);
    return pop(state);
}

Serializer::StateBase* Serializer::writeDenseArray(uint32_t numProperties, uint32_t length, Serializer::StateBase* state)
{
    m_writer.writeDenseArray(numProperties, length);
    return pop(state);
}

Serializer::StateBase* Serializer::handleError(Serializer::Status errorStatus, const String& message, Serializer::StateBase* state)
{
    ASSERT(errorStatus != Success);
    m_status = errorStatus;
    m_errorMessage = message;
    while (state) {
        StateBase* tmp = state->nextState();
        delete state;
        state = tmp;
    }
    return new ErrorState;
}

bool Serializer::checkComposite(Serializer::StateBase* top)
{
    ASSERT(top);
    if (m_depth > maxDepth)
        return false;
    if (!shouldCheckForCycles(m_depth))
        return true;
    v8::Handle<v8::Value> composite = top->composite();
    for (StateBase* state = top->nextState(); state; state = state->nextState()) {
        if (state->composite() == composite)
            return false;
    }
    return true;
}

void Serializer::writeString(v8::Handle<v8::Value> value)
{
    v8::Handle<v8::String> string = value.As<v8::String>();
    if (!string->Length() || string->IsOneByte())
        m_writer.writeOneByteString(string);
    else
        m_writer.writeUCharString(string);
}

void Serializer::writeStringObject(v8::Handle<v8::Value> value)
{
    v8::Handle<v8::StringObject> stringObject = value.As<v8::StringObject>();
    v8::String::Utf8Value stringValue(stringObject->ValueOf());
    m_writer.writeStringObject(*stringValue, stringValue.length());
}

void Serializer::writeNumberObject(v8::Handle<v8::Value> value)
{
    v8::Handle<v8::NumberObject> numberObject = value.As<v8::NumberObject>();
    m_writer.writeNumberObject(numberObject->ValueOf());
}

void Serializer::writeBooleanObject(v8::Handle<v8::Value> value)
{
    v8::Handle<v8::BooleanObject> booleanObject = value.As<v8::BooleanObject>();
    m_writer.writeBooleanObject(booleanObject->ValueOf());
}

Serializer::StateBase* Serializer::writeBlob(v8::Handle<v8::Value> value, Serializer::StateBase* next)
{
    Blob* blob = V8Blob::toImpl(value.As<v8::Object>());
    if (!blob)
        return 0;
    if (blob->hasBeenClosed())
        return handleError(DataCloneError, "A Blob object has been closed, and could therefore not be cloned.", next);
    int blobIndex = -1;
    m_blobDataHandles.set(blob->uuid(), blob->blobDataHandle());
    if (appendBlobInfo(blob->uuid(), blob->type(), blob->size(), &blobIndex))
        m_writer.writeBlobIndex(blobIndex);
    else
        m_writer.writeBlob(blob->uuid(), blob->type(), blob->size());
    return 0;
}

Serializer::StateBase* Serializer::writeDOMFileSystem(v8::Handle<v8::Value> value, StateBase* next)
{
    DOMFileSystem* fs = V8DOMFileSystem::toImpl(value.As<v8::Object>());
    if (!fs)
        return 0;
    if (!fs->clonable())
        return handleError(DataCloneError, "A FileSystem object could not be cloned.", next);
    m_writer.writeDOMFileSystem(fs->type(), fs->name(), fs->rootURL().string());
    return 0;
}

Serializer::StateBase* Serializer::writeFile(v8::Handle<v8::Value> value, Serializer::StateBase* next)
{
    File* file = V8File::toImpl(value.As<v8::Object>());
    if (!file)
        return 0;
    if (file->hasBeenClosed())
        return handleError(DataCloneError, "A File object has been closed, and could therefore not be cloned.", next);
    int blobIndex = -1;
    m_blobDataHandles.set(file->uuid(), file->blobDataHandle());
    if (appendFileInfo(file, &blobIndex)) {
        ASSERT(blobIndex >= 0);
        m_writer.writeFileIndex(blobIndex);
    } else {
        m_writer.writeFile(*file);
    }
    return 0;
}

Serializer::StateBase* Serializer::writeFileList(v8::Handle<v8::Value> value, Serializer::StateBase* next)
{
    FileList* fileList = V8FileList::toImpl(value.As<v8::Object>());
    if (!fileList)
        return 0;
    unsigned length = fileList->length();
    Vector<int> blobIndices;
    for (unsigned i = 0; i < length; ++i) {
        int blobIndex = -1;
        const File* file = fileList->item(i);
        if (file->hasBeenClosed())
            return handleError(DataCloneError, "A File object has been closed, and could therefore not be cloned.", next);
        m_blobDataHandles.set(file->uuid(), file->blobDataHandle());
        if (appendFileInfo(file, &blobIndex)) {
            ASSERT(!i || blobIndex > 0);
            ASSERT(blobIndex >= 0);
            blobIndices.append(blobIndex);
        }
    }
    if (!blobIndices.isEmpty())
        m_writer.writeFileListIndex(blobIndices);
    else
        m_writer.writeFileList(*fileList);
    return 0;
}

bool Serializer::writeCryptoKey(v8::Handle<v8::Value> value)
{
    CryptoKey* key = V8CryptoKey::toImpl(value.As<v8::Object>());
    if (!key)
        return false;
    return m_writer.writeCryptoKey(key->key());
}

void Serializer::writeImageData(v8::Handle<v8::Value> value)
{
    ImageData* imageData = V8ImageData::toImpl(value.As<v8::Object>());
    if (!imageData)
        return;
    Uint8ClampedArray* pixelArray = imageData->data();
    m_writer.writeImageData(imageData->width(), imageData->height(), pixelArray->data(), pixelArray->length());
}

void Serializer::writeRegExp(v8::Handle<v8::Value> value)
{
    v8::Handle<v8::RegExp> regExp = value.As<v8::RegExp>();
    m_writer.writeRegExp(regExp->GetSource(), regExp->GetFlags());
}

Serializer::StateBase* Serializer::writeAndGreyArrayBufferView(v8::Handle<v8::Object> object, Serializer::StateBase* next)
{
    ASSERT(!object.IsEmpty());
    DOMArrayBufferView* arrayBufferView = V8ArrayBufferView::toImpl(object);
    if (!arrayBufferView)
        return 0;
    if (!arrayBufferView->buffer())
        return handleError(DataCloneError, "An ArrayBuffer could not be cloned.", next);
    v8::Handle<v8::Value> underlyingBuffer = toV8(arrayBufferView->buffer(), m_scriptState->context()->Global(), isolate());
    if (underlyingBuffer.IsEmpty())
        return handleError(DataCloneError, "An ArrayBuffer could not be cloned.", next);
    StateBase* stateOut = doSerializeArrayBuffer(underlyingBuffer, next);
    if (stateOut)
        return stateOut;
    m_writer.writeArrayBufferView(*arrayBufferView->view());
    // This should be safe: we serialize something that we know to be a wrapper (see
    // the toV8 call above), so the call to doSerializeArrayBuffer should neither
    // cause the system stack to overflow nor should it have potential to reach
    // this ArrayBufferView again.
    //
    // We do need to grey the underlying buffer before we grey its view, however;
    // ArrayBuffers may be shared, so they need to be given reference IDs, and an
    // ArrayBufferView cannot be constructed without a corresponding ArrayBuffer
    // (or without an additional tag that would allow us to do two-stage construction
    // like we do for Objects and Arrays).
    greyObject(object);
    return 0;
}

Serializer::StateBase* Serializer::writeArrayBuffer(v8::Handle<v8::Value> value, Serializer::StateBase* next)
{
    DOMArrayBuffer* arrayBuffer = V8ArrayBuffer::toImpl(value.As<v8::Object>());
    if (!arrayBuffer)
        return 0;
    if (arrayBuffer->isNeutered())
        return handleError(DataCloneError, "An ArrayBuffer is neutered and could not be cloned.", next);
    ASSERT(!m_transferredArrayBuffers.contains(value.As<v8::Object>()));
    m_writer.writeArrayBuffer(*arrayBuffer->buffer());
    return 0;
}

Serializer::StateBase* Serializer::writeTransferredArrayBuffer(v8::Handle<v8::Value> value, uint32_t index, Serializer::StateBase* next)
{
    DOMArrayBuffer* arrayBuffer = V8ArrayBuffer::toImpl(value.As<v8::Object>());
    if (!arrayBuffer)
        return 0;
    if (arrayBuffer->isNeutered())
        return handleError(DataCloneError, "An ArrayBuffer is neutered and could not be cloned.", next);
    m_writer.writeTransferredArrayBuffer(index);
    return 0;
}

bool Serializer::shouldSerializeDensely(uint32_t length, uint32_t propertyCount)
{
    // Let K be the cost of serializing all property values that are there
    // Cost of serializing sparsely: 5*propertyCount + K (5 bytes per uint32_t key)
    // Cost of serializing densely: K + 1*(length - propertyCount) (1 byte for all properties that are not there)
    // so densely is better than sparsly whenever 6*propertyCount > length
    return 6 * propertyCount >= length;
}

Serializer::StateBase* Serializer::startArrayState(v8::Handle<v8::Array> array, Serializer::StateBase* next)
{
    v8::Handle<v8::Array> propertyNames = array->GetPropertyNames();
    if (StateBase* newState = checkException(next))
        return newState;
    uint32_t length = array->Length();

    if (shouldSerializeDensely(length, propertyNames->Length())) {
        m_writer.writeGenerateFreshDenseArray(length);
        return push(new DenseArrayState(array, propertyNames, next, isolate()));
    }

    m_writer.writeGenerateFreshSparseArray(length);
    return push(new SparseArrayState(array, propertyNames, next, isolate()));
}

Serializer::StateBase* Serializer::startObjectState(v8::Handle<v8::Object> object, Serializer::StateBase* next)
{
    m_writer.writeGenerateFreshObject();
    // FIXME: check not a wrapper
    return push(new ObjectState(object, next));
}

// Marks object as having been visited by the serializer and assigns it a unique object reference ID.
// An object may only be greyed once.
void Serializer::greyObject(const v8::Handle<v8::Object>& object)
{
    ASSERT(!m_objectPool.contains(object));
    uint32_t objectReference = m_nextObjectReference++;
    m_objectPool.set(object, objectReference);
}

bool Serializer::appendBlobInfo(const String& uuid, const String& type, unsigned long long size, int* index)
{
    if (!m_blobInfo)
        return false;
    *index = m_blobInfo->size();
    m_blobInfo->append(WebBlobInfo(uuid, type, size));
    return true;
}

bool Serializer::appendFileInfo(const File* file, int* index)
{
    if (!m_blobInfo)
        return false;

    long long size = -1;
    double lastModified = invalidFileTime();
    file->captureSnapshot(size, lastModified);
    *index = m_blobInfo->size();
    m_blobInfo->append(WebBlobInfo(file->uuid(), file->path(), file->name(), file->type(), lastModified, size));
    return true;
}

bool Reader::read(v8::Handle<v8::Value>* value, CompositeCreator& creator)
{
    SerializationTag tag;
    if (!readTag(&tag))
        return false;
    switch (tag) {
    case ReferenceCountTag: {
        if (!m_version)
            return false;
        uint32_t referenceTableSize;
        if (!doReadUint32(&referenceTableSize))
            return false;
        // If this test fails, then the serializer and deserializer disagree about the assignment
        // of object reference IDs. On the deserialization side, this means there are too many or too few
        // calls to pushObjectReference.
        if (referenceTableSize != creator.objectReferenceCount())
            return false;
        return true;
    }
    case InvalidTag:
        return false;
    case PaddingTag:
        return true;
    case UndefinedTag:
        *value = v8::Undefined(isolate());
        break;
    case NullTag:
        *value = v8::Null(isolate());
        break;
    case TrueTag:
        *value = v8Boolean(true, isolate());
        break;
    case FalseTag:
        *value = v8Boolean(false, isolate());
        break;
    case TrueObjectTag:
        *value = v8::BooleanObject::New(true);
        creator.pushObjectReference(*value);
        break;
    case FalseObjectTag:
        *value = v8::BooleanObject::New(false);
        creator.pushObjectReference(*value);
        break;
    case StringTag:
        if (!readString(value))
            return false;
        break;
    case StringUCharTag:
        if (!readUCharString(value))
            return false;
        break;
    case StringObjectTag:
        if (!readStringObject(value))
            return false;
        creator.pushObjectReference(*value);
        break;
    case Int32Tag:
        if (!readInt32(value))
            return false;
        break;
    case Uint32Tag:
        if (!readUint32(value))
            return false;
        break;
    case DateTag:
        if (!readDate(value))
            return false;
        creator.pushObjectReference(*value);
        break;
    case NumberTag:
        if (!readNumber(value))
            return false;
        break;
    case NumberObjectTag:
        if (!readNumberObject(value))
            return false;
        creator.pushObjectReference(*value);
        break;
    case BlobTag:
    case BlobIndexTag:
        if (!readBlob(value, tag == BlobIndexTag))
            return false;
        creator.pushObjectReference(*value);
        break;
    case FileTag:
    case FileIndexTag:
        if (!readFile(value, tag == FileIndexTag))
            return false;
        creator.pushObjectReference(*value);
        break;
    case DOMFileSystemTag:
        if (!readDOMFileSystem(value))
            return false;
        creator.pushObjectReference(*value);
        break;
    case FileListTag:
    case FileListIndexTag:
        if (!readFileList(value, tag == FileListIndexTag))
            return false;
        creator.pushObjectReference(*value);
        break;
    case CryptoKeyTag:
        if (!readCryptoKey(value))
            return false;
        creator.pushObjectReference(*value);
        break;
    case ImageDataTag:
        if (!readImageData(value))
            return false;
        creator.pushObjectReference(*value);
        break;

    case RegExpTag:
        if (!readRegExp(value))
            return false;
        creator.pushObjectReference(*value);
        break;
    case ObjectTag: {
        uint32_t numProperties;
        if (!doReadUint32(&numProperties))
            return false;
        if (!creator.completeObject(numProperties, value))
            return false;
        break;
    }
    case SparseArrayTag: {
        uint32_t numProperties;
        uint32_t length;
        if (!doReadUint32(&numProperties))
            return false;
        if (!doReadUint32(&length))
            return false;
        if (!creator.completeSparseArray(numProperties, length, value))
            return false;
        break;
    }
    case DenseArrayTag: {
        uint32_t numProperties;
        uint32_t length;
        if (!doReadUint32(&numProperties))
            return false;
        if (!doReadUint32(&length))
            return false;
        if (!creator.completeDenseArray(numProperties, length, value))
            return false;
        break;
    }
    case ArrayBufferViewTag: {
        if (!m_version)
            return false;
        if (!readArrayBufferView(value, creator))
            return false;
        creator.pushObjectReference(*value);
        break;
    }
    case ArrayBufferTag: {
        if (!m_version)
            return false;
        if (!readArrayBuffer(value))
            return false;
        creator.pushObjectReference(*value);
        break;
    }
    case GenerateFreshObjectTag: {
        if (!m_version)
            return false;
        if (!creator.newObject())
            return false;
        return true;
    }
    case GenerateFreshSparseArrayTag: {
        if (!m_version)
            return false;
        uint32_t length;
        if (!doReadUint32(&length))
            return false;
        if (!creator.newSparseArray(length))
            return false;
        return true;
    }
    case GenerateFreshDenseArrayTag: {
        if (!m_version)
            return false;
        uint32_t length;
        if (!doReadUint32(&length))
            return false;
        if (!creator.newDenseArray(length))
            return false;
        return true;
    }
    case MessagePortTag: {
        if (!m_version)
            return false;
        uint32_t index;
        if (!doReadUint32(&index))
            return false;
        if (!creator.tryGetTransferredMessagePort(index, value))
            return false;
        break;
    }
    case ArrayBufferTransferTag: {
        if (!m_version)
            return false;
        uint32_t index;
        if (!doReadUint32(&index))
            return false;
        if (!creator.tryGetTransferredArrayBuffer(index, value))
            return false;
        break;
    }
    case ObjectReferenceTag: {
        if (!m_version)
            return false;
        uint32_t reference;
        if (!doReadUint32(&reference))
            return false;
        if (!creator.tryGetObjectFromObjectReference(reference, value))
            return false;
        break;
    }
    default:
        return false;
    }
    return !value->IsEmpty();
}

bool Reader::readVersion(uint32_t& version)
{
    SerializationTag tag;
    if (!readTag(&tag)) {
        // This is a nullary buffer. We're still version 0.
        version = 0;
        return true;
    }
    if (tag != VersionTag) {
        // Versions of the format past 0 start with the version tag.
        version = 0;
        // Put back the tag.
        undoReadTag();
        return true;
    }
    // Version-bearing messages are obligated to finish the version tag.
    return doReadUint32(&version);
}

void Reader::setVersion(uint32_t version)
{
    m_version = version;
}

bool Reader::readTag(SerializationTag* tag)
{
    if (m_position >= m_length)
        return false;
    *tag = static_cast<SerializationTag>(m_buffer[m_position++]);
    return true;
}

void Reader::undoReadTag()
{
    if (m_position > 0)
        --m_position;
}

bool Reader::readArrayBufferViewSubTag(ArrayBufferViewSubTag* tag)
{
    if (m_position >= m_length)
        return false;
    *tag = static_cast<ArrayBufferViewSubTag>(m_buffer[m_position++]);
    return true;
}

bool Reader::readString(v8::Handle<v8::Value>* value)
{
    uint32_t length;
    if (!doReadUint32(&length))
        return false;
    if (m_position + length > m_length)
        return false;
    *value = v8::String::NewFromUtf8(isolate(), reinterpret_cast<const char*>(m_buffer + m_position), v8::String::kNormalString, length);
    m_position += length;
    return true;
}

bool Reader::readUCharString(v8::Handle<v8::Value>* value)
{
    uint32_t length;
    if (!doReadUint32(&length) || (length & 1))
        return false;
    if (m_position + length > m_length)
        return false;
    ASSERT(!(m_position & 1));
    *value = v8::String::NewFromTwoByte(isolate(), reinterpret_cast<const uint16_t*>(m_buffer + m_position), v8::String::kNormalString, length / sizeof(UChar));
    m_position += length;
    return true;
}

bool Reader::readStringObject(v8::Handle<v8::Value>* value)
{
    v8::Handle<v8::Value> stringValue;
    if (!readString(&stringValue) || !stringValue->IsString())
        return false;
    *value = v8::StringObject::New(stringValue.As<v8::String>());
    return true;
}

bool Reader::readWebCoreString(String* string)
{
    uint32_t length;
    if (!doReadUint32(&length))
        return false;
    if (m_position + length > m_length)
        return false;
    *string = String::fromUTF8(reinterpret_cast<const char*>(m_buffer + m_position), length);
    m_position += length;
    return true;
}

bool Reader::readInt32(v8::Handle<v8::Value>* value)
{
    uint32_t rawValue;
    if (!doReadUint32(&rawValue))
        return false;
    *value = v8::Integer::New(isolate(), static_cast<int32_t>(ZigZag::decode(rawValue)));
    return true;
}

bool Reader::readUint32(v8::Handle<v8::Value>* value)
{
    uint32_t rawValue;
    if (!doReadUint32(&rawValue))
        return false;
    *value = v8::Integer::NewFromUnsigned(isolate(), rawValue);
    return true;
}

bool Reader::readDate(v8::Handle<v8::Value>* value)
{
    double numberValue;
    if (!doReadNumber(&numberValue))
        return false;
    *value = v8DateOrNaN(numberValue, isolate());
    return true;
}

bool Reader::readNumber(v8::Handle<v8::Value>* value)
{
    double number;
    if (!doReadNumber(&number))
        return false;
    *value = v8::Number::New(isolate(), number);
    return true;
}

bool Reader::readNumberObject(v8::Handle<v8::Value>* value)
{
    double number;
    if (!doReadNumber(&number))
        return false;
    *value = v8::NumberObject::New(isolate(), number);
    return true;
}

bool Reader::readImageData(v8::Handle<v8::Value>* value)
{
    uint32_t width;
    uint32_t height;
    uint32_t pixelDataLength;
    if (!doReadUint32(&width))
        return false;
    if (!doReadUint32(&height))
        return false;
    if (!doReadUint32(&pixelDataLength))
        return false;
    if (m_position + pixelDataLength > m_length)
        return false;
    RefPtrWillBeRawPtr<ImageData> imageData = ImageData::create(IntSize(width, height));
    Uint8ClampedArray* pixelArray = imageData->data();
    ASSERT(pixelArray);
    ASSERT(pixelArray->length() >= pixelDataLength);
    memcpy(pixelArray->data(), m_buffer + m_position, pixelDataLength);
    m_position += pixelDataLength;
    *value = toV8(imageData.release(), m_scriptState->context()->Global(), isolate());
    return true;
}

PassRefPtr<ArrayBuffer> Reader::doReadArrayBuffer()
{
    uint32_t byteLength;
    if (!doReadUint32(&byteLength))
        return nullptr;
    if (m_position + byteLength > m_length)
        return nullptr;
    const void* bufferStart = m_buffer + m_position;
    m_position += byteLength;
    return ArrayBuffer::create(bufferStart, byteLength);
}

bool Reader::readArrayBuffer(v8::Handle<v8::Value>* value)
{
    RefPtr<ArrayBuffer> arrayBuffer = doReadArrayBuffer();
    if (!arrayBuffer)
        return false;
    *value = toV8(DOMArrayBuffer::create(arrayBuffer.release()), m_scriptState->context()->Global(), isolate());
    return true;
}

bool Reader::readArrayBufferView(v8::Handle<v8::Value>* value, CompositeCreator& creator)
{
    ArrayBufferViewSubTag subTag;
    uint32_t byteOffset;
    uint32_t byteLength;
    RefPtr<DOMArrayBuffer> arrayBuffer;
    v8::Handle<v8::Value> arrayBufferV8Value;
    if (!readArrayBufferViewSubTag(&subTag))
        return false;
    if (!doReadUint32(&byteOffset))
        return false;
    if (!doReadUint32(&byteLength))
        return false;
    if (!creator.consumeTopOfStack(&arrayBufferV8Value))
        return false;
    if (arrayBufferV8Value.IsEmpty())
        return false;
    arrayBuffer = V8ArrayBuffer::toImpl(arrayBufferV8Value.As<v8::Object>());
    if (!arrayBuffer)
        return false;

    v8::Handle<v8::Object> creationContext = m_scriptState->context()->Global();
    switch (subTag) {
    case ByteArrayTag:
        *value = toV8(DOMInt8Array::create(arrayBuffer.release(), byteOffset, byteLength), creationContext, isolate());
        break;
    case UnsignedByteArrayTag:
        *value = toV8(DOMUint8Array::create(arrayBuffer.release(), byteOffset, byteLength), creationContext,  isolate());
        break;
    case UnsignedByteClampedArrayTag:
        *value = toV8(DOMUint8ClampedArray::create(arrayBuffer.release(), byteOffset, byteLength), creationContext, isolate());
        break;
    case ShortArrayTag: {
        uint32_t shortLength = byteLength / sizeof(int16_t);
        if (shortLength * sizeof(int16_t) != byteLength)
            return false;
        *value = toV8(DOMInt16Array::create(arrayBuffer.release(), byteOffset, shortLength), creationContext, isolate());
        break;
    }
    case UnsignedShortArrayTag: {
        uint32_t shortLength = byteLength / sizeof(uint16_t);
        if (shortLength * sizeof(uint16_t) != byteLength)
            return false;
        *value = toV8(DOMUint16Array::create(arrayBuffer.release(), byteOffset, shortLength), creationContext, isolate());
        break;
    }
    case IntArrayTag: {
        uint32_t intLength = byteLength / sizeof(int32_t);
        if (intLength * sizeof(int32_t) != byteLength)
            return false;
        *value = toV8(DOMInt32Array::create(arrayBuffer.release(), byteOffset, intLength), creationContext, isolate());
        break;
    }
    case UnsignedIntArrayTag: {
        uint32_t intLength = byteLength / sizeof(uint32_t);
        if (intLength * sizeof(uint32_t) != byteLength)
            return false;
        *value = toV8(DOMUint32Array::create(arrayBuffer.release(), byteOffset, intLength), creationContext, isolate());
        break;
    }
    case FloatArrayTag: {
        uint32_t floatLength = byteLength / sizeof(float);
        if (floatLength * sizeof(float) != byteLength)
            return false;
        *value = toV8(DOMFloat32Array::create(arrayBuffer.release(), byteOffset, floatLength), creationContext, isolate());
        break;
    }
    case DoubleArrayTag: {
        uint32_t floatLength = byteLength / sizeof(double);
        if (floatLength * sizeof(double) != byteLength)
            return false;
        *value = toV8(DOMFloat64Array::create(arrayBuffer.release(), byteOffset, floatLength), creationContext, isolate());
        break;
    }
    case DataViewTag:
        *value = toV8(DOMDataView::create(arrayBuffer.release(), byteOffset, byteLength), creationContext, isolate());
        break;
    default:
        return false;
    }
    // The various *Array::create() methods will return null if the range the view expects is
    // mismatched with the range the buffer can provide or if the byte offset is not aligned
    // to the size of the element type.
    return !value->IsEmpty();
}

bool Reader::readRegExp(v8::Handle<v8::Value>* value)
{
    v8::Handle<v8::Value> pattern;
    if (!readString(&pattern))
        return false;
    uint32_t flags;
    if (!doReadUint32(&flags))
        return false;
    *value = v8::RegExp::New(pattern.As<v8::String>(), static_cast<v8::RegExp::Flags>(flags));
    return true;
}

bool Reader::readBlob(v8::Handle<v8::Value>* value, bool isIndexed)
{
    if (m_version < 3)
        return false;
    Blob* blob = nullptr;
    if (isIndexed) {
        if (m_version < 6)
            return false;
        ASSERT(m_blobInfo);
        uint32_t index;
        if (!doReadUint32(&index) || index >= m_blobInfo->size())
            return false;
        const WebBlobInfo& info = (*m_blobInfo)[index];
        blob = Blob::create(getOrCreateBlobDataHandle(info.uuid(), info.type(), info.size()));
    } else {
        ASSERT(!m_blobInfo);
        String uuid;
        String type;
        uint64_t size;
        ASSERT(!m_blobInfo);
        if (!readWebCoreString(&uuid))
            return false;
        if (!readWebCoreString(&type))
            return false;
        if (!doReadUint64(&size))
            return false;
        blob = Blob::create(getOrCreateBlobDataHandle(uuid, type, size));
    }
    *value = toV8(blob, m_scriptState->context()->Global(), isolate());
    return true;
}

bool Reader::readDOMFileSystem(v8::Handle<v8::Value>* value)
{
    uint32_t type;
    String name;
    String url;
    if (!doReadUint32(&type))
        return false;
    if (!readWebCoreString(&name))
        return false;
    if (!readWebCoreString(&url))
        return false;
    DOMFileSystem* fs = DOMFileSystem::create(m_scriptState->executionContext(), name, static_cast<FileSystemType>(type), KURL(ParsedURLString, url));
    *value = toV8(fs, m_scriptState->context()->Global(), isolate());
    return true;
}

bool Reader::readFile(v8::Handle<v8::Value>* value, bool isIndexed)
{
    File* file = nullptr;
    if (isIndexed) {
        if (m_version < 6)
            return false;
        file = readFileIndexHelper();
    } else {
        file = readFileHelper();
    }
    if (!file)
        return false;
    *value = toV8(file, m_scriptState->context()->Global(), isolate());
    return true;
}

bool Reader::readFileList(v8::Handle<v8::Value>* value, bool isIndexed)
{
    if (m_version < 3)
        return false;
    uint32_t length;
    if (!doReadUint32(&length))
        return false;
    FileList* fileList = FileList::create();
    for (unsigned i = 0; i < length; ++i) {
        File* file = nullptr;
        if (isIndexed) {
            if (m_version < 6)
                return false;
            file = readFileIndexHelper();
        } else {
            file = readFileHelper();
        }
        if (!file)
            return false;
        fileList->append(file);
    }
    *value = toV8(fileList, m_scriptState->context()->Global(), isolate());
    return true;
}

bool Reader::readCryptoKey(v8::Handle<v8::Value>* value)
{
    uint32_t rawKeyType;
    if (!doReadUint32(&rawKeyType))
        return false;

    WebCryptoKeyAlgorithm algorithm;
    WebCryptoKeyType type = WebCryptoKeyTypeSecret;

    switch (static_cast<CryptoKeySubTag>(rawKeyType)) {
    case AesKeyTag:
        if (!doReadAesKey(algorithm, type))
            return false;
        break;
    case HmacKeyTag:
        if (!doReadHmacKey(algorithm, type))
            return false;
        break;
    case RsaHashedKeyTag:
        if (!doReadRsaHashedKey(algorithm, type))
            return false;
        break;
    case EcKeyTag:
        if (!doReadEcKey(algorithm, type))
            return false;
        break;
    default:
        return false;
    }

    WebCryptoKeyUsageMask usages;
    bool extractable;
    if (!doReadKeyUsages(usages, extractable))
        return false;

    uint32_t keyDataLength;
    if (!doReadUint32(&keyDataLength))
        return false;

    if (m_position + keyDataLength > m_length)
        return false;

    const uint8_t* keyData = m_buffer + m_position;
    m_position += keyDataLength;

    WebCryptoKey key = WebCryptoKey::createNull();
    if (!Platform::current()->crypto()->deserializeKeyForClone(
        algorithm, type, extractable, usages, keyData, keyDataLength, key)) {
        return false;
    }

    *value = toV8(CryptoKey::create(key), m_scriptState->context()->Global(), isolate());
    return true;
}

File* Reader::readFileHelper()
{
    if (m_version < 3)
        return nullptr;
    ASSERT(!m_blobInfo);
    String path;
    String name;
    String relativePath;
    String uuid;
    String type;
    uint32_t hasSnapshot = 0;
    uint64_t size = 0;
    double lastModified = 0;
    if (!readWebCoreString(&path))
        return nullptr;
    if (m_version >= 4 && !readWebCoreString(&name))
        return nullptr;
    if (m_version >= 4 && !readWebCoreString(&relativePath))
        return nullptr;
    if (!readWebCoreString(&uuid))
        return nullptr;
    if (!readWebCoreString(&type))
        return nullptr;
    if (m_version >= 4 && !doReadUint32(&hasSnapshot))
        return nullptr;
    if (hasSnapshot) {
        if (!doReadUint64(&size))
            return nullptr;
        if (!doReadNumber(&lastModified))
            return nullptr;
    }
    uint32_t isUserVisible = 1;
    if (m_version >= 7 && !doReadUint32(&isUserVisible))
        return nullptr;
    const File::UserVisibility userVisibility = (isUserVisible > 0) ? File::IsUserVisible : File::IsNotUserVisible;
    return File::createFromSerialization(path, name, relativePath, userVisibility, hasSnapshot > 0, size, lastModified, getOrCreateBlobDataHandle(uuid, type));
}

File* Reader::readFileIndexHelper()
{
    if (m_version < 3)
        return nullptr;
    ASSERT(m_blobInfo);
    uint32_t index;
    if (!doReadUint32(&index) || index >= m_blobInfo->size())
        return nullptr;
    const WebBlobInfo& info = (*m_blobInfo)[index];
    return File::createFromIndexedSerialization(info.filePath(), info.fileName(), info.size(), info.lastModified(), getOrCreateBlobDataHandle(info.uuid(), info.type(), info.size()));
}

bool Reader::doReadUint32(uint32_t* value)
{
    return doReadUintHelper(value);
}

bool Reader::doReadUint64(uint64_t* value)
{
    return doReadUintHelper(value);
}

bool Reader::doReadNumber(double* number)
{
    if (m_position + sizeof(double) > m_length)
        return false;
    uint8_t* numberAsByteArray = reinterpret_cast<uint8_t*>(number);
    for (unsigned i = 0; i < sizeof(double); ++i)
        numberAsByteArray[i] = m_buffer[m_position++];
    return true;
}

PassRefPtr<BlobDataHandle> Reader::getOrCreateBlobDataHandle(const String& uuid, const String& type, long long size)
{
    // The containing ssv may have a BDH for this uuid if this ssv is just being
    // passed from main to worker thread (for example). We use those values when creating
    // the new blob instead of cons'ing up a new BDH.
    //
    // FIXME: Maybe we should require that it work that way where the ssv must have a BDH for any
    // blobs it comes across during deserialization. Would require callers to explicitly populate
    // the collection of BDH's for blobs to work, which would encourage lifetimes to be considered
    // when passing ssv's around cross process. At present, we get 'lucky' in some cases because
    // the blob in the src process happens to still exist at the time the dest process is deserializing.
    // For example in sharedWorker.postMessage(...).
    BlobDataHandleMap::const_iterator it = m_blobDataHandles.find(uuid);
    if (it != m_blobDataHandles.end()) {
        // make assertions about type and size?
        return it->value;
    }
    return BlobDataHandle::create(uuid, type, size);
}

bool Reader::doReadHmacKey(WebCryptoKeyAlgorithm& algorithm, WebCryptoKeyType& type)
{
    uint32_t lengthBytes;
    if (!doReadUint32(&lengthBytes))
        return false;
    WebCryptoAlgorithmId hash;
    if (!doReadAlgorithmId(hash))
        return false;
    algorithm = WebCryptoKeyAlgorithm::createHmac(hash, lengthBytes * 8);
    type = WebCryptoKeyTypeSecret;
    return !algorithm.isNull();
}

bool Reader::doReadAesKey(WebCryptoKeyAlgorithm& algorithm, WebCryptoKeyType& type)
{
    WebCryptoAlgorithmId id;
    if (!doReadAlgorithmId(id))
        return false;
    uint32_t lengthBytes;
    if (!doReadUint32(&lengthBytes))
        return false;
    algorithm = WebCryptoKeyAlgorithm::createAes(id, lengthBytes * 8);
    type = WebCryptoKeyTypeSecret;
    return !algorithm.isNull();
}

bool Reader::doReadRsaHashedKey(WebCryptoKeyAlgorithm& algorithm, WebCryptoKeyType& type)
{
    WebCryptoAlgorithmId id;
    if (!doReadAlgorithmId(id))
        return false;

    if (!doReadAsymmetricKeyType(type))
        return false;

    uint32_t modulusLengthBits;
    if (!doReadUint32(&modulusLengthBits))
        return false;

    uint32_t publicExponentSize;
    if (!doReadUint32(&publicExponentSize))
        return false;

    if (m_position + publicExponentSize > m_length)
        return false;

    const uint8_t* publicExponent = m_buffer + m_position;
    m_position += publicExponentSize;

    WebCryptoAlgorithmId hash;
    if (!doReadAlgorithmId(hash))
        return false;
    algorithm = WebCryptoKeyAlgorithm::createRsaHashed(id, modulusLengthBits, publicExponent, publicExponentSize, hash);

    return !algorithm.isNull();
}

bool Reader::doReadEcKey(WebCryptoKeyAlgorithm& algorithm, WebCryptoKeyType& type)
{
    WebCryptoAlgorithmId id;
    if (!doReadAlgorithmId(id))
        return false;

    if (!doReadAsymmetricKeyType(type))
        return false;

    WebCryptoNamedCurve namedCurve;
    if (!doReadNamedCurve(namedCurve))
        return false;

    algorithm = WebCryptoKeyAlgorithm::createEc(id, namedCurve);
    return !algorithm.isNull();
}

bool Reader::doReadAlgorithmId(WebCryptoAlgorithmId& id)
{
    uint32_t rawId;
    if (!doReadUint32(&rawId))
        return false;

    switch (static_cast<CryptoKeyAlgorithmTag>(rawId)) {
    case AesCbcTag:
        id = WebCryptoAlgorithmIdAesCbc;
        return true;
    case HmacTag:
        id = WebCryptoAlgorithmIdHmac;
        return true;
    case RsaSsaPkcs1v1_5Tag:
        id = WebCryptoAlgorithmIdRsaSsaPkcs1v1_5;
        return true;
    case Sha1Tag:
        id = WebCryptoAlgorithmIdSha1;
        return true;
    case Sha256Tag:
        id = WebCryptoAlgorithmIdSha256;
        return true;
    case Sha384Tag:
        id = WebCryptoAlgorithmIdSha384;
        return true;
    case Sha512Tag:
        id = WebCryptoAlgorithmIdSha512;
        return true;
    case AesGcmTag:
        id = WebCryptoAlgorithmIdAesGcm;
        return true;
    case RsaOaepTag:
        id = WebCryptoAlgorithmIdRsaOaep;
        return true;
    case AesCtrTag:
        id = WebCryptoAlgorithmIdAesCtr;
        return true;
    case AesKwTag:
        id = WebCryptoAlgorithmIdAesKw;
        return true;
    case RsaPssTag:
        id = WebCryptoAlgorithmIdRsaPss;
        return true;
    case EcdsaTag:
        id = WebCryptoAlgorithmIdEcdsa;
        return true;
    }

    return false;
}

bool Reader::doReadAsymmetricKeyType(WebCryptoKeyType& type)
{
    uint32_t rawType;
    if (!doReadUint32(&rawType))
        return false;

    switch (static_cast<AssymetricCryptoKeyType>(rawType)) {
    case PublicKeyType:
        type = WebCryptoKeyTypePublic;
        return true;
    case PrivateKeyType:
        type = WebCryptoKeyTypePrivate;
        return true;
    }

    return false;
}

bool Reader::doReadNamedCurve(WebCryptoNamedCurve& namedCurve)
{
    uint32_t rawName;
    if (!doReadUint32(&rawName))
        return false;

    switch (static_cast<NamedCurveTag>(rawName)) {
    case P256Tag:
        namedCurve = WebCryptoNamedCurveP256;
        return true;
    case P384Tag:
        namedCurve = WebCryptoNamedCurveP384;
        return true;
    case P521Tag:
        namedCurve = WebCryptoNamedCurveP521;
        return true;
    }

    return false;
}

bool Reader::doReadKeyUsages(WebCryptoKeyUsageMask& usages, bool& extractable)
{
    // Reminder to update this when adding new key usages.
    COMPILE_ASSERT(EndOfWebCryptoKeyUsage == (1 << 7) + 1, UpdateMe);
    const uint32_t allPossibleUsages = ExtractableUsage | EncryptUsage | DecryptUsage | SignUsage | VerifyUsage | DeriveKeyUsage | WrapKeyUsage | UnwrapKeyUsage | DeriveBitsUsage;

    uint32_t rawUsages;
    if (!doReadUint32(&rawUsages))
        return false;

    // Make sure it doesn't contain an unrecognized usage value.
    if (rawUsages & ~allPossibleUsages)
        return false;

    usages = 0;

    extractable = rawUsages & ExtractableUsage;

    if (rawUsages & EncryptUsage)
        usages |= WebCryptoKeyUsageEncrypt;
    if (rawUsages & DecryptUsage)
        usages |= WebCryptoKeyUsageDecrypt;
    if (rawUsages & SignUsage)
        usages |= WebCryptoKeyUsageSign;
    if (rawUsages & VerifyUsage)
        usages |= WebCryptoKeyUsageVerify;
    if (rawUsages & DeriveKeyUsage)
        usages |= WebCryptoKeyUsageDeriveKey;
    if (rawUsages & WrapKeyUsage)
        usages |= WebCryptoKeyUsageWrapKey;
    if (rawUsages & UnwrapKeyUsage)
        usages |= WebCryptoKeyUsageUnwrapKey;
    if (rawUsages & DeriveBitsUsage)
        usages |= WebCryptoKeyUsageDeriveBits;

    return true;
}

v8::Handle<v8::Value> Deserializer::deserialize()
{
    v8::Isolate* isolate = m_reader.scriptState()->isolate();
    if (!m_reader.readVersion(m_version) || m_version > SerializedScriptValue::wireFormatVersion)
        return v8::Null(isolate);
    m_reader.setVersion(m_version);
    v8::EscapableHandleScope scope(isolate);
    while (!m_reader.isEof()) {
        if (!doDeserialize())
            return v8::Null(isolate);
    }
    if (stackDepth() != 1 || m_openCompositeReferenceStack.size())
        return v8::Null(isolate);
    v8::Handle<v8::Value> result = scope.Escape(element(0));
    return result;
}

bool Deserializer::newSparseArray(uint32_t)
{
    v8::Local<v8::Array> array = v8::Array::New(m_reader.scriptState()->isolate(), 0);
    openComposite(array);
    return true;
}

bool Deserializer::newDenseArray(uint32_t length)
{
    v8::Local<v8::Array> array = v8::Array::New(m_reader.scriptState()->isolate(), length);
    openComposite(array);
    return true;
}

bool Deserializer::consumeTopOfStack(v8::Handle<v8::Value>* object)
{
    if (stackDepth() < 1)
        return false;
    *object = element(stackDepth() - 1);
    pop(1);
    return true;
}

bool Deserializer::newObject()
{
    v8::Local<v8::Object> object = v8::Object::New(m_reader.scriptState()->isolate());
    if (object.IsEmpty())
        return false;
    openComposite(object);
    return true;
}

bool Deserializer::completeObject(uint32_t numProperties, v8::Handle<v8::Value>* value)
{
    v8::Local<v8::Object> object;
    if (m_version > 0) {
        v8::Local<v8::Value> composite;
        if (!closeComposite(&composite))
            return false;
        object = composite.As<v8::Object>();
    } else {
        object = v8::Object::New(m_reader.scriptState()->isolate());
    }
    if (object.IsEmpty())
        return false;
    return initializeObject(object, numProperties, value);
}

bool Deserializer::completeSparseArray(uint32_t numProperties, uint32_t length, v8::Handle<v8::Value>* value)
{
    v8::Local<v8::Array> array;
    if (m_version > 0) {
        v8::Local<v8::Value> composite;
        if (!closeComposite(&composite))
            return false;
        array = composite.As<v8::Array>();
    } else {
        array = v8::Array::New(m_reader.scriptState()->isolate());
    }
    if (array.IsEmpty())
        return false;
    return initializeObject(array, numProperties, value);
}

bool Deserializer::completeDenseArray(uint32_t numProperties, uint32_t length, v8::Handle<v8::Value>* value)
{
    v8::Local<v8::Array> array;
    if (m_version > 0) {
        v8::Local<v8::Value> composite;
        if (!closeComposite(&composite))
            return false;
        array = composite.As<v8::Array>();
    }
    if (array.IsEmpty())
        return false;
    if (!initializeObject(array, numProperties, value))
        return false;
    if (length > stackDepth())
        return false;
    for (unsigned i = 0, stackPos = stackDepth() - length; i < length; i++, stackPos++) {
        v8::Local<v8::Value> elem = element(stackPos);
        if (!elem->IsUndefined())
            array->Set(i, elem);
    }
    pop(length);
    return true;
}

void Deserializer::pushObjectReference(const v8::Handle<v8::Value>& object)
{
    m_objectPool.append(object);
}

bool Deserializer::tryGetTransferredMessagePort(uint32_t index, v8::Handle<v8::Value>* object)
{
    if (!m_transferredMessagePorts)
        return false;
    if (index >= m_transferredMessagePorts->size())
        return false;
    v8::Handle<v8::Object> creationContext = m_reader.scriptState()->context()->Global();
    *object = toV8(m_transferredMessagePorts->at(index).get(), creationContext, m_reader.scriptState()->isolate());
    return true;
}

bool Deserializer::tryGetTransferredArrayBuffer(uint32_t index, v8::Handle<v8::Value>* object)
{
    if (!m_arrayBufferContents)
        return false;
    if (index >= m_arrayBuffers.size())
        return false;
    v8::Handle<v8::Object> result = m_arrayBuffers.at(index);
    if (result.IsEmpty()) {
        RefPtr<DOMArrayBuffer> buffer = DOMArrayBuffer::create(m_arrayBufferContents->at(index));
        v8::Isolate* isolate = m_reader.scriptState()->isolate();
        v8::Handle<v8::Object> creationContext = m_reader.scriptState()->context()->Global();
        result = toV8Object(buffer.get(), creationContext, isolate);
        m_arrayBuffers[index] = result;
    }
    *object = result;
    return true;
}

bool Deserializer::tryGetObjectFromObjectReference(uint32_t reference, v8::Handle<v8::Value>* object)
{
    if (reference >= m_objectPool.size())
        return false;
    *object = m_objectPool[reference];
    return object;
}

uint32_t Deserializer::objectReferenceCount()
{
    return m_objectPool.size();
}

bool Deserializer::initializeObject(v8::Handle<v8::Object> object, uint32_t numProperties, v8::Handle<v8::Value>* value)
{
    unsigned length = 2 * numProperties;
    if (length > stackDepth())
        return false;
    for (unsigned i = stackDepth() - length; i < stackDepth(); i += 2) {
        v8::Local<v8::Value> propertyName = element(i);
        v8::Local<v8::Value> propertyValue = element(i + 1);
        object->Set(propertyName, propertyValue);
    }
    pop(length);
    *value = object;
    return true;
}

bool Deserializer::read(v8::Local<v8::Value>* value)
{
    return m_reader.read(value, *this);
}

bool Deserializer::doDeserialize()
{
    v8::Local<v8::Value> value;
    if (!read(&value))
        return false;
    if (!value.IsEmpty())
        push(value);
    return true;
}

v8::Local<v8::Value> Deserializer::element(unsigned index)
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < m_stack.size());
    return m_stack[index];
}

void Deserializer::openComposite(const v8::Local<v8::Value>& object)
{
    uint32_t newObjectReference = m_objectPool.size();
    m_openCompositeReferenceStack.append(newObjectReference);
    m_objectPool.append(object);
}

bool Deserializer::closeComposite(v8::Handle<v8::Value>* object)
{
    if (!m_openCompositeReferenceStack.size())
        return false;
    uint32_t objectReference = m_openCompositeReferenceStack[m_openCompositeReferenceStack.size() - 1];
    m_openCompositeReferenceStack.shrink(m_openCompositeReferenceStack.size() - 1);
    if (objectReference >= m_objectPool.size())
        return false;
    *object = m_objectPool[objectReference];
    return true;
}

} // SerializedScriptValueInternal

} // namespace blink
