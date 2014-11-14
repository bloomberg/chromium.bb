// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptValueSerializer_h
#define ScriptValueSerializer_h

#include "bindings/core/v8/SerializationTag.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "public/platform/WebCrypto.h"
#include "public/platform/WebCryptoKey.h"
#include "public/platform/WebCryptoKeyAlgorithm.h"
#include "wtf/ArrayBufferContents.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <v8.h>

namespace WTF {

class ArrayBuffer;
class ArrayBufferView;

}

namespace blink {

class File;
class FileList;

namespace SerializedScriptValueInternal {

typedef Vector<WTF::ArrayBufferContents, 1> ArrayBufferContentsArray;

// V8ObjectMap is a map from V8 objects to arbitrary values of type T.
// V8 objects (or handles to V8 objects) cannot be used as keys in ordinary wtf::HashMaps;
// this class should be used instead. GCObject must be a subtype of v8::Object.
// Suggested usage:
//     V8ObjectMap<v8::Object, int> map;
//     v8::Handle<v8::Object> obj = ...;
//     map.set(obj, 42);
template<typename GCObject, typename T>
class V8ObjectMap {
public:
    bool contains(const v8::Handle<GCObject>& handle)
    {
        return m_map.contains(*handle);
    }

    bool tryGet(const v8::Handle<GCObject>& handle, T* valueOut)
    {
        typename HandleToT::iterator result = m_map.find(*handle);
        if (result != m_map.end()) {
            *valueOut = result->value;
            return true;
        }
        return false;
    }

    void set(const v8::Handle<GCObject>& handle, const T& value)
    {
        m_map.set(*handle, value);
    }

private:
    // This implementation uses GetIdentityHash(), which sets a hidden property on the object containing
    // a random integer (or returns the one that had been previously set). This ensures that the table
    // never needs to be rebuilt across garbage collections at the expense of doing additional allocation
    // and making more round trips into V8. Note that since GetIdentityHash() is defined only on
    // v8::Objects, this V8ObjectMap cannot be used to map v8::Strings to T (because the public V8 API
    // considers a v8::String to be a v8::Primitive).

    // If V8 exposes a way to get at the address of the object held by a handle, then we can produce
    // an alternate implementation that does not need to do any V8-side allocation; however, it will
    // need to rehash after every garbage collection because a key object may have been moved.
    template<typename G>
    struct V8HandlePtrHash {
        static v8::Handle<G> unsafeHandleFromRawValue(const G* value)
        {
            const v8::Handle<G>* handle = reinterpret_cast<const v8::Handle<G>*>(&value);
            return *handle;
        }

        static unsigned hash(const G* key)
        {
            return static_cast<unsigned>(unsafeHandleFromRawValue(key)->GetIdentityHash());
        }
        static bool equal(const G* a, const G* b)
        {
            return unsafeHandleFromRawValue(a) == unsafeHandleFromRawValue(b);
        }
        // For HashArg.
        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    typedef WTF::HashMap<GCObject*, T, V8HandlePtrHash<GCObject> > HandleToT;
    HandleToT m_map;
};

// Writer is responsible for serializing primitive types and storing
// information used to reconstruct composite types.
class Writer {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(Writer);
public:
    typedef UChar BufferValueType;

    Writer()
        : m_position(0)
    {
    }

    // Write functions for primitive types.

    void writeUndefined();
    void writeNull();
    void writeTrue();
    void writeFalse();
    void writeBooleanObject(bool value);
    void writeOneByteString(v8::Handle<v8::String>&);
    void writeUCharString(v8::Handle<v8::String>&);
    void writeStringObject(const char* data, int length);
    void writeWebCoreString(const String&);
    void writeVersion();
    void writeInt32(int32_t value);
    void writeUint32(uint32_t value);
    void writeDate(double numberValue);
    void writeNumber(double number);
    void writeNumberObject(double number);
    void writeBlob(const String& uuid, const String& type, unsigned long long size);
    void writeBlobIndex(int blobIndex);
    void writeDOMFileSystem(int type, const String& name, const String& url);
    void writeFile(const File&);
    void writeFileIndex(int blobIndex);
    void writeFileList(const FileList&);
    void writeFileListIndex(const Vector<int>& blobIndices);
    bool writeCryptoKey(const WebCryptoKey&);
    void writeArrayBuffer(const ArrayBuffer&);
    void writeArrayBufferView(const ArrayBufferView&);
    void writeImageData(uint32_t width, uint32_t height, const uint8_t* pixelData, uint32_t pixelDataLength);
    void writeRegExp(v8::Local<v8::String> pattern, v8::RegExp::Flags);
    void writeTransferredMessagePort(uint32_t index);
    void writeTransferredArrayBuffer(uint32_t index);
    void writeObjectReference(uint32_t reference);
    void writeObject(uint32_t numProperties);
    void writeSparseArray(uint32_t numProperties, uint32_t length);
    void writeDenseArray(uint32_t numProperties, uint32_t length);
    String takeWireString();
    void writeReferenceCount(uint32_t numberOfReferences);
    void writeGenerateFreshObject();
    void writeGenerateFreshSparseArray(uint32_t length);
    void writeGenerateFreshDenseArray(uint32_t length);

protected:
    void doWriteFile(const File&);
    void doWriteArrayBuffer(const ArrayBuffer&);
    void doWriteString(const char* data, int length);
    void doWriteWebCoreString(const String&);
    void doWriteHmacKey(const WebCryptoKey&);
    void doWriteAesKey(const WebCryptoKey&);
    void doWriteRsaHashedKey(const WebCryptoKey&);
    void doWriteEcKey(const WebCryptoKey&);
    void doWriteAlgorithmId(WebCryptoAlgorithmId);
    void doWriteAsymmetricKeyType(WebCryptoKeyType);
    void doWriteNamedCurve(WebCryptoNamedCurve);
    void doWriteKeyUsages(const WebCryptoKeyUsageMask usages, bool extractable);
    int bytesNeededToWireEncode(uint32_t value);

    template<class T>
    void doWriteUintHelper(T value)
    {
        while (true) {
            uint8_t b = (value & SerializedScriptValue::varIntMask);
            value >>= SerializedScriptValue::varIntShift;
            if (!value) {
                append(b);
                break;
            }
            append(b | (1 << SerializedScriptValue::varIntShift));
        }
    }

    void doWriteUint32(uint32_t value);
    void doWriteUint64(uint64_t value);
    void doWriteNumber(double number);
    void append(SerializationTag);
    void append(uint8_t b);
    void append(const uint8_t* data, int length);
    void ensureSpace(unsigned extra);
    void fillHole();
    uint8_t* byteAt(int position);
    int v8StringWriteOptions();

private:
    Vector<BufferValueType> m_buffer;
    unsigned m_position;
};

class Serializer {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(Serializer);
protected:
    class StateBase;
public:
    enum Status {
        Success,
        InputError,
        DataCloneError,
        JSException
    };

    Serializer(Writer&, MessagePortArray* messagePorts, ArrayBufferArray* arrayBuffers, WebBlobInfoArray*, BlobDataHandleMap& blobDataHandles, v8::TryCatch&, ScriptState*);
    v8::Isolate* isolate() { return m_scriptState->isolate(); }

    Status serialize(v8::Handle<v8::Value>);
    String errorMessage() { return m_errorMessage; }

    // Functions used by serialization states.
    Serializer::StateBase* doSerialize(v8::Handle<v8::Value>, Serializer::StateBase* next);

    StateBase* doSerializeArrayBuffer(v8::Handle<v8::Value> arrayBuffer, StateBase* next);
    StateBase* checkException(StateBase*);
    StateBase* writeObject(uint32_t numProperties, StateBase*);
    StateBase* writeSparseArray(uint32_t numProperties, uint32_t length, StateBase*);
    StateBase* writeDenseArray(uint32_t numProperties, uint32_t length, StateBase*);

protected:
    class StateBase {
        WTF_MAKE_NONCOPYABLE(StateBase);
    public:
        virtual ~StateBase() { }

        // Link to the next state to form a stack.
        StateBase* nextState() { return m_next; }

        // Composite object we're processing in this state.
        v8::Handle<v8::Value> composite() { return m_composite; }

        // Serializes (a part of) the current composite and returns
        // the next state to process or null when this is the final
        // state.
        virtual StateBase* advance(Serializer&) = 0;

    protected:
        StateBase(v8::Handle<v8::Value> composite, StateBase* next)
            : m_composite(composite)
            , m_next(next)
        {
        }

    private:
        v8::Handle<v8::Value> m_composite;
        StateBase* m_next;
    };

    // Dummy state that is used to signal serialization errors.
    class ErrorState final : public StateBase {
    public:
        ErrorState()
            : StateBase(v8Undefined(), 0)
        {
        }

        virtual StateBase* advance(Serializer&) override
        {
            delete this;
            return 0;
        }
    };

    template <typename T>
    class State : public StateBase {
    public:
        v8::Handle<T> composite() { return v8::Handle<T>::Cast(StateBase::composite()); }

    protected:
        State(v8::Handle<T> composite, StateBase* next)
            : StateBase(composite, next)
        {
        }
    };

    class AbstractObjectState : public State<v8::Object> {
    public:
        AbstractObjectState(v8::Handle<v8::Object> object, StateBase* next)
            : State<v8::Object>(object, next)
            , m_index(0)
            , m_numSerializedProperties(0)
            , m_nameDone(false)
        {
        }

    protected:
        virtual StateBase* objectDone(unsigned numProperties, Serializer&) = 0;

        StateBase* serializeProperties(bool ignoreIndexed, Serializer&);
        v8::Local<v8::Array> m_propertyNames;

    private:
        v8::Local<v8::Value> m_propertyName;
        unsigned m_index;
        unsigned m_numSerializedProperties;
        bool m_nameDone;
    };

    class ObjectState final : public AbstractObjectState {
    public:
        ObjectState(v8::Handle<v8::Object> object, StateBase* next)
            : AbstractObjectState(object, next)
        {
        }

        virtual StateBase* advance(Serializer&) override;

    protected:
        virtual StateBase* objectDone(unsigned numProperties, Serializer&) override;
    };

    class DenseArrayState final : public AbstractObjectState {
    public:
        DenseArrayState(v8::Handle<v8::Array> array, v8::Handle<v8::Array> propertyNames, StateBase* next, v8::Isolate* isolate)
            : AbstractObjectState(array, next)
            , m_arrayIndex(0)
            , m_arrayLength(array->Length())
        {
            m_propertyNames = v8::Local<v8::Array>::New(isolate, propertyNames);
        }

        virtual StateBase* advance(Serializer&) override;

    protected:
        virtual StateBase* objectDone(unsigned numProperties, Serializer&) override;

    private:
        uint32_t m_arrayIndex;
        uint32_t m_arrayLength;
    };

    class SparseArrayState final : public AbstractObjectState {
    public:
        SparseArrayState(v8::Handle<v8::Array> array, v8::Handle<v8::Array> propertyNames, StateBase* next, v8::Isolate* isolate)
            : AbstractObjectState(array, next)
        {
            m_propertyNames = v8::Local<v8::Array>::New(isolate, propertyNames);
        }

        virtual StateBase* advance(Serializer&) override;

    protected:
        virtual StateBase* objectDone(unsigned numProperties, Serializer&) override;
    };

private:
    StateBase* push(StateBase* state)
    {
        ASSERT(state);
        ++m_depth;
        return checkComposite(state) ? state : handleError(InputError, "Value being cloned is either cyclic or too deeply nested.", state);
    }

    StateBase* pop(StateBase* state)
    {
        ASSERT(state);
        --m_depth;
        StateBase* next = state->nextState();
        delete state;
        return next;
    }

    bool checkComposite(StateBase* top);
    void writeString(v8::Handle<v8::Value>);
    void writeStringObject(v8::Handle<v8::Value>);
    void writeNumberObject(v8::Handle<v8::Value>);
    void writeBooleanObject(v8::Handle<v8::Value>);
    StateBase* writeBlob(v8::Handle<v8::Value>, StateBase* next);
    StateBase* writeDOMFileSystem(v8::Handle<v8::Value>, StateBase* next);
    StateBase* writeFile(v8::Handle<v8::Value>, StateBase* next);
    StateBase* writeFileList(v8::Handle<v8::Value>, StateBase* next);
    bool writeCryptoKey(v8::Handle<v8::Value>);
    void writeImageData(v8::Handle<v8::Value>);
    void writeRegExp(v8::Handle<v8::Value>);
    StateBase* writeAndGreyArrayBufferView(v8::Handle<v8::Object>, StateBase* next);
    StateBase* writeArrayBuffer(v8::Handle<v8::Value>, StateBase* next);
    StateBase* writeTransferredArrayBuffer(v8::Handle<v8::Value>, uint32_t index, StateBase* next);
    static bool shouldSerializeDensely(uint32_t length, uint32_t propertyCount);

    StateBase* startArrayState(v8::Handle<v8::Array>, StateBase* next);
    StateBase* startObjectState(v8::Handle<v8::Object>, StateBase* next);

    // Marks object as having been visited by the serializer and assigns it a unique object reference ID.
    // An object may only be greyed once.
    void greyObject(const v8::Handle<v8::Object>&);
    bool appendBlobInfo(const String& uuid, const String& type, unsigned long long size, int* index);
    bool appendFileInfo(const File*, int* index);

protected:
    StateBase* handleError(Status errorStatus, const String& message, StateBase*);

    Writer& writer() { return m_writer; }
    uint32_t nextObjectReference() const { return m_nextObjectReference; }

private:
    RefPtr<ScriptState> m_scriptState;
    Writer& m_writer;
    v8::TryCatch& m_tryCatch;
    int m_depth;
    Status m_status;
    String m_errorMessage;
    typedef V8ObjectMap<v8::Object, uint32_t> ObjectPool;
    ObjectPool m_objectPool;
    ObjectPool m_transferredMessagePorts;
    ObjectPool m_transferredArrayBuffers;
    uint32_t m_nextObjectReference;
    WebBlobInfoArray* m_blobInfo;
    BlobDataHandleMap& m_blobDataHandles;
};

// Interface used by Reader to create objects of composite types.
class CompositeCreator {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(CompositeCreator);
public:
    CompositeCreator() { }
    virtual ~CompositeCreator() { }

    virtual bool consumeTopOfStack(v8::Handle<v8::Value>*) = 0;
    virtual uint32_t objectReferenceCount() = 0;
    virtual void pushObjectReference(const v8::Handle<v8::Value>&) = 0;
    virtual bool tryGetObjectFromObjectReference(uint32_t reference, v8::Handle<v8::Value>*) = 0;
    virtual bool tryGetTransferredMessagePort(uint32_t index, v8::Handle<v8::Value>*) = 0;
    virtual bool tryGetTransferredArrayBuffer(uint32_t index, v8::Handle<v8::Value>*) = 0;
    virtual bool newSparseArray(uint32_t length) = 0;
    virtual bool newDenseArray(uint32_t length) = 0;
    virtual bool newObject() = 0;
    virtual bool completeObject(uint32_t numProperties, v8::Handle<v8::Value>*) = 0;
    virtual bool completeSparseArray(uint32_t numProperties, uint32_t length, v8::Handle<v8::Value>*) = 0;
    virtual bool completeDenseArray(uint32_t numProperties, uint32_t length, v8::Handle<v8::Value>*) = 0;
};

// Reader is responsible for deserializing primitive types and
// restoring information about saved objects of composite types.
class Reader {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(Reader);
public:
    Reader(const uint8_t* buffer, int length, const WebBlobInfoArray* blobInfo, BlobDataHandleMap& blobDataHandles, ScriptState* scriptState)
        : m_scriptState(scriptState)
        , m_buffer(buffer)
        , m_length(length)
        , m_position(0)
        , m_version(0)
        , m_blobInfo(blobInfo)
        , m_blobDataHandles(blobDataHandles)
    {
        ASSERT(!(reinterpret_cast<size_t>(buffer) & 1));
        ASSERT(length >= 0);
    }

    bool isEof() const { return m_position >= m_length; }

    ScriptState* scriptState() const { return m_scriptState.get(); }

protected:
    v8::Isolate* isolate() const { return m_scriptState->isolate(); }
    unsigned length() const { return m_length; }
    unsigned position() const { return m_position; }

    const uint8_t* allocate(uint32_t size)
    {
        const uint8_t* allocated = m_buffer + m_position;
        m_position += size;
        return allocated;
    }

public:
    bool read(v8::Handle<v8::Value>*, CompositeCreator&);
    bool readVersion(uint32_t& version);
    void setVersion(uint32_t);

private:
    bool readTag(SerializationTag*);
    bool readWebCoreString(String*);
    bool readUint32(v8::Handle<v8::Value>*);
    void undoReadTag();
    bool readArrayBufferViewSubTag(ArrayBufferViewSubTag*);
    bool readString(v8::Handle<v8::Value>*);
    bool readUCharString(v8::Handle<v8::Value>*);
    bool readStringObject(v8::Handle<v8::Value>*);
    bool readInt32(v8::Handle<v8::Value>*);
    bool readDate(v8::Handle<v8::Value>*);
    bool readNumber(v8::Handle<v8::Value>*);
    bool readNumberObject(v8::Handle<v8::Value>*);
    bool readImageData(v8::Handle<v8::Value>*);
    PassRefPtr<ArrayBuffer> doReadArrayBuffer();
    bool readArrayBuffer(v8::Handle<v8::Value>*);
    bool readArrayBufferView(v8::Handle<v8::Value>*, CompositeCreator&);
    bool readRegExp(v8::Handle<v8::Value>*);
    bool readBlob(v8::Handle<v8::Value>*, bool isIndexed);
    bool readDOMFileSystem(v8::Handle<v8::Value>*);
    bool readFile(v8::Handle<v8::Value>*, bool isIndexed);
    bool readFileList(v8::Handle<v8::Value>*, bool isIndexed);
    bool readCryptoKey(v8::Handle<v8::Value>*);
    File* readFileHelper();
    File* readFileIndexHelper();

    template<class T>
    bool doReadUintHelper(T* value)
    {
        *value = 0;
        uint8_t currentByte;
        int shift = 0;
        do {
            if (m_position >= m_length)
                return false;
            currentByte = m_buffer[m_position++];
            *value |= ((currentByte & SerializedScriptValue::varIntMask) << shift);
            shift += SerializedScriptValue::varIntShift;
        } while (currentByte & (1 << SerializedScriptValue::varIntShift));
        return true;
    }

    bool doReadUint32(uint32_t* value);
    bool doReadUint64(uint64_t* value);
    bool doReadNumber(double* number);
    PassRefPtr<BlobDataHandle> getOrCreateBlobDataHandle(const String& uuid, const String& type, long long size = -1);
    bool doReadHmacKey(WebCryptoKeyAlgorithm&, WebCryptoKeyType&);
    bool doReadAesKey(WebCryptoKeyAlgorithm&, WebCryptoKeyType&);
    bool doReadRsaHashedKey(WebCryptoKeyAlgorithm&, WebCryptoKeyType&);
    bool doReadEcKey(WebCryptoKeyAlgorithm&, WebCryptoKeyType&);
    bool doReadAlgorithmId(WebCryptoAlgorithmId&);
    bool doReadAsymmetricKeyType(WebCryptoKeyType&);
    bool doReadNamedCurve(WebCryptoNamedCurve&);
    bool doReadKeyUsages(WebCryptoKeyUsageMask& usages, bool& extractable);

private:
    RefPtr<ScriptState> m_scriptState;
    const uint8_t* m_buffer;
    const unsigned m_length;
    unsigned m_position;
    uint32_t m_version;
    const WebBlobInfoArray* m_blobInfo;
    const BlobDataHandleMap& m_blobDataHandles;
};

class Deserializer : public CompositeCreator {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(Deserializer);
public:
    Deserializer(Reader& reader, MessagePortArray* messagePorts, ArrayBufferContentsArray* arrayBufferContents)
        : m_reader(reader)
        , m_transferredMessagePorts(messagePorts)
        , m_arrayBufferContents(arrayBufferContents)
        , m_arrayBuffers(arrayBufferContents ? arrayBufferContents->size() : 0)
        , m_version(0)
    {
    }

    v8::Handle<v8::Value> deserialize();
    virtual bool newSparseArray(uint32_t) override;
    virtual bool newDenseArray(uint32_t length) override;
    virtual bool consumeTopOfStack(v8::Handle<v8::Value>*) override;
    virtual bool newObject() override;
    virtual bool completeObject(uint32_t numProperties, v8::Handle<v8::Value>*) override;
    virtual bool completeSparseArray(uint32_t numProperties, uint32_t length, v8::Handle<v8::Value>*) override;
    virtual bool completeDenseArray(uint32_t numProperties, uint32_t length, v8::Handle<v8::Value>*) override;
    virtual void pushObjectReference(const v8::Handle<v8::Value>&) override;
    virtual bool tryGetTransferredMessagePort(uint32_t index, v8::Handle<v8::Value>*) override;
    virtual bool tryGetTransferredArrayBuffer(uint32_t index, v8::Handle<v8::Value>*) override;
    virtual bool tryGetObjectFromObjectReference(uint32_t reference, v8::Handle<v8::Value>*) override;
    virtual uint32_t objectReferenceCount() override;

protected:
    Reader& reader() { return m_reader; }
    bool read(v8::Local<v8::Value>*);

private:
    bool initializeObject(v8::Handle<v8::Object>, uint32_t numProperties, v8::Handle<v8::Value>*);
    bool doDeserialize();
    void push(v8::Local<v8::Value> value) { m_stack.append(value); };
    void pop(unsigned length)
    {
        ASSERT(length <= m_stack.size());
        m_stack.shrink(m_stack.size() - length);
    }
    unsigned stackDepth() const { return m_stack.size(); }

    v8::Local<v8::Value> element(unsigned index);
    void openComposite(const v8::Local<v8::Value>&);
    bool closeComposite(v8::Handle<v8::Value>*);

    Reader& m_reader;
    Vector<v8::Local<v8::Value> > m_stack;
    Vector<v8::Handle<v8::Value> > m_objectPool;
    Vector<uint32_t> m_openCompositeReferenceStack;
    RawPtrWillBeMember<MessagePortArray> m_transferredMessagePorts;
    ArrayBufferContentsArray* m_arrayBufferContents;
    Vector<v8::Handle<v8::Object> > m_arrayBuffers;
    uint32_t m_version;
};

} // namespace SerializedScriptValueInternal

} // namespace blink

#endif // ScriptValueSerializer_h
