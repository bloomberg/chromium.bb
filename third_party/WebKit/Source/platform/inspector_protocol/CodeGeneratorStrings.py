# Copyright (c) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# THis file contains string resources for CodeGenerator.
# Its syntax is a Python syntax subset, suitable for manual parsing.

frontend_domain_class = (
"""    class PLATFORM_EXPORT $domainClassName {
    public:
        static $domainClassName* from(Frontend* frontend) { return &(frontend->m_$domainFieldName) ;}
        $domainClassName(FrontendChannel* frontendChannel) : m_frontendChannel(frontendChannel) { }
${frontendDomainMethodDeclarations}
        void flush() { m_frontendChannel->flush(); }
    private:
        FrontendChannel* m_frontendChannel;
    };

""")

backend_method = (
"""void DispatcherImpl::${domainName}_$methodName(int sessionId, int callId, JSONObject*$requestMessageObject, JSONArray* protocolErrors)
{
    if (!$agentField)
        protocolErrors->pushString("${domainName} handler is not available.");
$methodCode
    if (protocolErrors->length()) {
        reportProtocolError(sessionId, callId, InvalidParams, String::format(InvalidParamsFormatString, commandName($commandNameIndex)), protocolErrors);
        return;
    }
$agentCallParamsDeclaration
    $agentField->$methodName($agentCallParams);
$responseCook
    sendResponse(sessionId, callId, $sendResponseCallParams);
}
""")

frontend_method = ("""void Frontend::$domainName::$eventName($parameters)
{
    RefPtr<JSONObject> jsonMessage = JSONObject::create();
    jsonMessage->setString("method", "$domainName.$eventName");
$code    if (m_frontendChannel)
        m_frontendChannel->sendProtocolNotification(jsonMessage.release());
}
""")

callback_main_methods = ("""
Dispatcher::$agentName::$callbackName::$callbackName(PassRefPtr<DispatcherImpl> backendImpl, int sessionId, int id) : CallbackBase(backendImpl, sessionId, id) {}

void Dispatcher::$agentName::$callbackName::sendSuccess($parameters)
{
    RefPtr<JSONObject> jsonMessage = JSONObject::create();
$code    sendIfActive(jsonMessage, ErrorString(), PassRefPtr<JSONValue>());
}
""")

callback_failure_method = (
"""void Dispatcher::$agentName::$callbackName::sendFailure(const ErrorString& error, $parameter)
{
    ASSERT(error.length());
    RefPtr<JSONValue> errorDataValue;
    if (error) {
        errorDataValue = $argument;
    }
    sendIfActive(nullptr, error, errorDataValue.release());
}
""")


frontend_h = (
"""#ifndef Frontend_h
#define Frontend_h

#include "platform/inspector_protocol/TypeBuilder.h"

#include "platform/JSONValues.h"
#include "platform/PlatformExport.h"
#include "platform/inspector_protocol/FrontendChannel.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {
namespace protocol {

using ErrorString = String;

class PLATFORM_EXPORT Frontend {
public:
    Frontend(FrontendChannel*);
    FrontendChannel* channel() { return m_frontendChannel; }

$domainClassList
private:
    FrontendChannel* m_frontendChannel;
${fieldDeclarations}};

} // namespace protocol
} // namespace blink
#endif // !defined(Frontend_h)
""")

backend_h = (
"""#ifndef Dispatcher_h
#define Dispatcher_h

#include "platform/inspector_protocol/TypeBuilder.h"

#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace blink {

class JSONObject;
class JSONArray;

namespace protocol {

class FrontendChannel;

using ErrorString = String;

class DispatcherImpl;

class PLATFORM_EXPORT Dispatcher: public RefCounted<Dispatcher> {
public:
    static PassRefPtr<Dispatcher> create(FrontendChannel* frontendChannel);
    virtual ~Dispatcher() { }

    class PLATFORM_EXPORT CallbackBase: public RefCounted<CallbackBase> {
    public:
        CallbackBase(PassRefPtr<DispatcherImpl> backendImpl, int sessionId, int id);
        virtual ~CallbackBase();
        void sendFailure(const ErrorString&);
        bool isActive();

    protected:
        void sendIfActive(PassRefPtr<JSONObject> partialMessage, const ErrorString& invocationError, PassRefPtr<JSONValue> errorData);

    private:
        void disable() { m_alreadySent = true; }

        RefPtr<DispatcherImpl> m_backendImpl;
        int m_sessionId;
        int m_id;
        bool m_alreadySent;

        friend class DispatcherImpl;
    };

$agentInterfaces
$virtualSetters

    virtual void clearFrontend() = 0;

    enum CommonErrorCode {
        ParseError = 0,
        InvalidRequest,
        MethodNotFound,
        InvalidParams,
        InternalError,
        ServerError,
        LastEntry,
    };

    void reportProtocolError(int sessionId, int callId, CommonErrorCode, const String& errorMessage) const;
    virtual void reportProtocolError(int sessionId, int callId, CommonErrorCode, const String& errorMessage, PassRefPtr<JSONValue> data) const = 0;
    virtual void dispatch(int sessionId, const String& message) = 0;
    static bool getCommandName(const String& message, String* result);

    enum MethodNames {
$methodNamesEnumContent

        kMethodNamesEnumSize
    };

    static const char* commandName(MethodNames);

private:
    static const char commandNames[];
    static const unsigned short commandNamesIndex[];
};

} // namespace protocol
} // namespace blink

using blink::protocol::ErrorString;

#endif // !defined(Dispatcher_h)


""")

backend_cpp = (
"""

#include "platform/inspector_protocol/Dispatcher.h"

#include "platform/JSONParser.h"
#include "platform/JSONValues.h"
#include "platform/inspector_protocol/FrontendChannel.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

namespace blink {
namespace protocol {

const char Dispatcher::commandNames[] = {
$methodNameDeclarations
};

const unsigned short Dispatcher::commandNamesIndex[] = {
$methodNameDeclarationsIndex
};

const char* Dispatcher::commandName(MethodNames index) {
    static_assert(static_cast<int>(kMethodNamesEnumSize) == WTF_ARRAY_LENGTH(commandNamesIndex), "MethodNames enum should have the same number of elements as commandNamesIndex");
    return commandNames + commandNamesIndex[index];
}

class DispatcherImpl : public Dispatcher {
public:
    DispatcherImpl(FrontendChannel* frontendChannel)
        : m_frontendChannel(frontendChannel)
$constructorInit
    {
        // Initialize dispatch map.
        const CallHandler handlers[] = {
  $messageHandlers
        };
        for (size_t i = 0; i < kMethodNamesEnumSize; ++i)
            m_dispatchMap.add(commandName(static_cast<MethodNames>(i)), handlers[i]);

        // Initialize common errors.
        m_commonErrors.insert(ParseError, -32700);
        m_commonErrors.insert(InvalidRequest, -32600);
        m_commonErrors.insert(MethodNotFound, -32601);
        m_commonErrors.insert(InvalidParams, -32602);
        m_commonErrors.insert(InternalError, -32603);
        m_commonErrors.insert(ServerError, -32000);
    }

    virtual void clearFrontend() { m_frontendChannel = 0; }
    virtual void dispatch(int sessionId, const String& message);
    virtual void reportProtocolError(int sessionId, int callId, CommonErrorCode, const String& errorMessage, PassRefPtr<JSONValue> data) const;
    using Dispatcher::reportProtocolError;

    void sendResponse(int sessionId, int callId, const ErrorString& invocationError, PassRefPtr<JSONValue> errorData, PassRefPtr<JSONObject> result);
    bool isActive() { return m_frontendChannel; }

$setters
private:
    using CallHandler = void (DispatcherImpl::*)(int sessionId, int callId, JSONObject* messageObject, JSONArray* protocolErrors);
    using DispatchMap = HashMap<String, CallHandler>;

$methodDeclarations

    FrontendChannel* m_frontendChannel;
$fieldDeclarations

    template<typename R, typename V, typename V0>
    static R getPropertyValueImpl(JSONObject* object, const char* name, bool* valueFound, JSONArray* protocolErrors, V0 initial_value, bool (*as_method)(JSONValue*, V*), const char* type_name);

    static int getInt(JSONObject* object, const char* name, bool* valueFound, JSONArray* protocolErrors);
    static double getDouble(JSONObject* object, const char* name, bool* valueFound, JSONArray* protocolErrors);
    static String getString(JSONObject* object, const char* name, bool* valueFound, JSONArray* protocolErrors);
    static bool getBoolean(JSONObject* object, const char* name, bool* valueFound, JSONArray* protocolErrors);
    static PassRefPtr<JSONObject> getObject(JSONObject* object, const char* name, bool* valueFound, JSONArray* protocolErrors);
    static PassRefPtr<JSONArray> getArray(JSONObject* object, const char* name, bool* valueFound, JSONArray* protocolErrors);

    void sendResponse(int sessionId, int callId, ErrorString invocationError, PassRefPtr<JSONObject> result)
    {
        sendResponse(sessionId, callId, invocationError, RefPtr<JSONValue>(), result);
    }
    void sendResponse(int sessionId, int callId, ErrorString invocationError)
    {
        sendResponse(sessionId, callId, invocationError, RefPtr<JSONValue>(), JSONObject::create());
    }
    static const char InvalidParamsFormatString[];

    DispatchMap m_dispatchMap;
    Vector<int> m_commonErrors;
};

const char DispatcherImpl::InvalidParamsFormatString[] = "Some arguments of method '%s' can't be processed";

$methods

PassRefPtr<Dispatcher> Dispatcher::create(FrontendChannel* frontendChannel)
{
    return adoptRef(new DispatcherImpl(frontendChannel));
}


void DispatcherImpl::dispatch(int sessionId, const String& message)
{
    RefPtr<Dispatcher> protect(this);
    int callId = 0;
    RefPtr<JSONValue> parsedMessage = parseJSON(message);
    ASSERT(parsedMessage);
    RefPtr<JSONObject> messageObject = parsedMessage->asObject();
    ASSERT(messageObject);

    RefPtr<JSONValue> callIdValue = messageObject->get("id");
    bool success = callIdValue->asNumber(&callId);
    ASSERT_UNUSED(success, success);

    RefPtr<JSONValue> methodValue = messageObject->get("method");
    String method;
    success = methodValue && methodValue->asString(&method);
    ASSERT_UNUSED(success, success);

    HashMap<String, CallHandler>::iterator it = m_dispatchMap.find(method);
    if (it == m_dispatchMap.end()) {
        reportProtocolError(sessionId, callId, MethodNotFound, "'" + method + "' wasn't found");
        return;
    }

    RefPtr<JSONArray> protocolErrors = JSONArray::create();
    ((*this).*it->value)(sessionId, callId, messageObject.get(), protocolErrors.get());
}

void DispatcherImpl::sendResponse(int sessionId, int callId, const ErrorString& invocationError, PassRefPtr<JSONValue> errorData, PassRefPtr<JSONObject> result)
{
    if (invocationError.length()) {
        reportProtocolError(sessionId, callId, ServerError, invocationError, errorData);
        return;
    }

    RefPtr<JSONObject> responseMessage = JSONObject::create();
    responseMessage->setNumber("id", callId);
    responseMessage->setObject("result", result);
    if (m_frontendChannel)
        m_frontendChannel->sendProtocolResponse(sessionId, callId, responseMessage.release());
}

void Dispatcher::reportProtocolError(int sessionId, int callId, CommonErrorCode code, const String& errorMessage) const
{
    reportProtocolError(sessionId, callId, code, errorMessage, PassRefPtr<JSONValue>());
}

void DispatcherImpl::reportProtocolError(int sessionId, int callId, CommonErrorCode code, const String& errorMessage, PassRefPtr<JSONValue> data) const
{
    ASSERT(code >=0);
    ASSERT((unsigned)code < m_commonErrors.size());
    ASSERT(m_commonErrors[code]);
    RefPtr<JSONObject> error = JSONObject::create();
    error->setNumber("code", m_commonErrors[code]);
    error->setString("message", errorMessage);
    ASSERT(error);
    if (data)
        error->setValue("data", data);
    RefPtr<JSONObject> message = JSONObject::create();
    message->setObject("error", error);
    message->setNumber("id", callId);
    if (m_frontendChannel)
        m_frontendChannel->sendProtocolResponse(sessionId, callId, message.release());
}

template<typename R, typename V, typename V0>
R DispatcherImpl::getPropertyValueImpl(JSONObject* object, const char* name, bool* valueFound, JSONArray* protocolErrors, V0 initial_value, bool (*as_method)(JSONValue*, V*), const char* type_name)
{
    ASSERT(protocolErrors);

    if (valueFound)
        *valueFound = false;

    V value = initial_value;

    if (!object) {
        if (!valueFound) {
            // Required parameter in missing params container.
            protocolErrors->pushString(String::format("'params' object must contain required parameter '%s' with type '%s'.", name, type_name));
        }
        return value;
    }

    JSONObject::const_iterator end = object->end();
    JSONObject::const_iterator valueIterator = object->find(name);

    if (valueIterator == end) {
        if (!valueFound)
            protocolErrors->pushString(String::format("Parameter '%s' with type '%s' was not found.", name, type_name));
        return value;
    }

    if (!as_method(valueIterator->value.get(), &value))
        protocolErrors->pushString(String::format("Parameter '%s' has wrong type. It must be '%s'.", name, type_name));
    else
        if (valueFound)
            *valueFound = true;
    return value;
}

struct AsMethodBridges {
    static bool asInt(JSONValue* value, int* output) { return value->asNumber(output); }
    static bool asDouble(JSONValue* value, double* output) { return value->asNumber(output); }
    static bool asString(JSONValue* value, String* output) { return value->asString(output); }
    static bool asBoolean(JSONValue* value, bool* output) { return value->asBoolean(output); }
    static bool asObject(JSONValue* value, RefPtr<JSONObject>* output) { return value->asObject(output); }
    static bool asArray(JSONValue* value, RefPtr<JSONArray>* output) { return value->asArray(output); }
};

int DispatcherImpl::getInt(JSONObject* object, const char* name, bool* valueFound, JSONArray* protocolErrors)
{
    return getPropertyValueImpl<int, int, int>(object, name, valueFound, protocolErrors, 0, AsMethodBridges::asInt, "Number");
}

double DispatcherImpl::getDouble(JSONObject* object, const char* name, bool* valueFound, JSONArray* protocolErrors)
{
    return getPropertyValueImpl<double, double, double>(object, name, valueFound, protocolErrors, 0, AsMethodBridges::asDouble, "Number");
}

String DispatcherImpl::getString(JSONObject* object, const char* name, bool* valueFound, JSONArray* protocolErrors)
{
    return getPropertyValueImpl<String, String, String>(object, name, valueFound, protocolErrors, "", AsMethodBridges::asString, "String");
}

bool DispatcherImpl::getBoolean(JSONObject* object, const char* name, bool* valueFound, JSONArray* protocolErrors)
{
    return getPropertyValueImpl<bool, bool, bool>(object, name, valueFound, protocolErrors, false, AsMethodBridges::asBoolean, "Boolean");
}

PassRefPtr<JSONObject> DispatcherImpl::getObject(JSONObject* object, const char* name, bool* valueFound, JSONArray* protocolErrors)
{
    return getPropertyValueImpl<PassRefPtr<JSONObject>, RefPtr<JSONObject>, JSONObject*>(object, name, valueFound, protocolErrors, 0, AsMethodBridges::asObject, "Object");
}

PassRefPtr<JSONArray> DispatcherImpl::getArray(JSONObject* object, const char* name, bool* valueFound, JSONArray* protocolErrors)
{
    return getPropertyValueImpl<PassRefPtr<JSONArray>, RefPtr<JSONArray>, JSONArray*>(object, name, valueFound, protocolErrors, 0, AsMethodBridges::asArray, "Array");
}

bool Dispatcher::getCommandName(const String& message, String* result)
{
    RefPtr<JSONValue> value = parseJSON(message);
    if (!value)
        return false;

    RefPtr<JSONObject> object = value->asObject();
    if (!object)
        return false;

    if (!object->getString("method", result))
        return false;

    return true;
}

Dispatcher::CallbackBase::CallbackBase(PassRefPtr<DispatcherImpl> backendImpl, int sessionId, int id)
    : m_backendImpl(backendImpl), m_sessionId(sessionId), m_id(id), m_alreadySent(false) {}

Dispatcher::CallbackBase::~CallbackBase() {}

void Dispatcher::CallbackBase::sendFailure(const ErrorString& error)
{
    ASSERT(error.length());
    sendIfActive(nullptr, error, PassRefPtr<JSONValue>());
}

bool Dispatcher::CallbackBase::isActive()
{
    return !m_alreadySent && m_backendImpl->isActive();
}

void Dispatcher::CallbackBase::sendIfActive(PassRefPtr<JSONObject> partialMessage, const ErrorString& invocationError, PassRefPtr<JSONValue> errorData)
{
    if (m_alreadySent)
        return;
    m_backendImpl->sendResponse(m_sessionId, m_id, invocationError, errorData, partialMessage);
    m_alreadySent = true;
}

} // namespace protocol
} // namespace blink

""")

frontend_cpp = (
"""

#include "platform/inspector_protocol/Frontend.h"

#include "platform/JSONValues.h"
#include "platform/inspector_protocol/FrontendChannel.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

namespace blink {
namespace protocol {

Frontend::Frontend(FrontendChannel* frontendChannel)
    : m_frontendChannel(frontendChannel)
    , $constructorInit
{
}

$methods

} // namespace protocol
} // namespace blink

""")

typebuilder_h = (
"""
#ifndef TypeBuilder_h
#define TypeBuilder_h

#include "platform/JSONValues.h"
#include "platform/PlatformExport.h"
#include "wtf/Assertions.h"
#include "wtf/PassRefPtr.h"

namespace blink {
namespace protocol {

namespace TypeBuilder {

template<typename T>
class OptOutput {
public:
    OptOutput() : m_assigned(false) { }

    void operator=(T value)
    {
        m_value = value;
        m_assigned = true;
    }

    bool isAssigned() { return m_assigned; }

    T getValue()
    {
        ASSERT(isAssigned());
        return m_value;
    }

private:
    T m_value;
    bool m_assigned;

    WTF_MAKE_NONCOPYABLE(OptOutput);
};

class RuntimeCastHelper {
public:
#if $validatorIfdefName
    template<JSONValue::Type TYPE>
    static void assertType(JSONValue* value)
    {
        ASSERT(value->type() == TYPE);
    }
    static void assertAny(JSONValue*);
    static void assertInt(JSONValue* value);
#endif
};


// This class provides "Traits" type for the input type T. It is programmed using C++ template specialization
// technique. By default it simply takes "ItemTraits" type from T, but it doesn't work with the base types.
template<typename T>
struct ArrayItemHelper {
    typedef typename T::ItemTraits Traits;
};

template<typename T>
class Array : public JSONArrayBase {
private:
    Array() { }

    JSONArray* openAccessors() {
        static_assert(sizeof(JSONArray) == sizeof(Array<T>), "JSONArray should be the same size as Array<T>");
        return static_cast<JSONArray*>(static_cast<JSONArrayBase*>(this));
    }

public:
    void addItem(PassRefPtr<T> value)
    {
        ArrayItemHelper<T>::Traits::pushRefPtr(this->openAccessors(), value);
    }

    void addItem(T value)
    {
        ArrayItemHelper<T>::Traits::pushRaw(this->openAccessors(), value);
    }

    static PassRefPtr<Array<T>> create()
    {
        return adoptRef(new Array<T>());
    }

    static PassRefPtr<Array<T>> runtimeCast(PassRefPtr<JSONValue> value)
    {
        RefPtr<JSONArray> array;
        bool castRes = value->asArray(&array);
        ASSERT_UNUSED(castRes, castRes);
#if $validatorIfdefName
        assertCorrectValue(array.get());
#endif  // $validatorIfdefName
        static_assert(sizeof(Array<T>) == sizeof(JSONArray), "Array<T> should be the same size as JSONArray");
        return static_cast<Array<T>*>(static_cast<JSONArrayBase*>(array.get()));
    }

    void concat(PassRefPtr<Array<T>> array)
    {
        return ArrayItemHelper<T>::Traits::concat(this->openAccessors(), array->openAccessors());
    }

#if $validatorIfdefName
    static void assertCorrectValue(JSONValue* value)
    {
        RefPtr<JSONArray> array;
        bool castRes = value->asArray(&array);
        ASSERT_UNUSED(castRes, castRes);
        for (unsigned i = 0; i < array->length(); i++)
            ArrayItemHelper<T>::Traits::template assertCorrectValue<T>(array->get(i).get());
    }

#endif // $validatorIfdefName
};

struct StructItemTraits {
    static void pushRefPtr(JSONArray* array, PassRefPtr<JSONValue> value)
    {
        array->pushValue(value);
    }

    static void concat(JSONArray* array, JSONArray* anotherArray)
    {
        for (JSONArray::iterator it = anotherArray->begin(); it != anotherArray->end(); ++it)
            array->pushValue(*it);
    }

#if $validatorIfdefName
    template<typename T>
    static void assertCorrectValue(JSONValue* value) {
        T::assertCorrectValue(value);
    }
#endif  // $validatorIfdefName
};

template<>
struct ArrayItemHelper<String> {
    struct Traits {
        static void pushRaw(JSONArray* array, const String& value)
        {
            array->pushString(value);
        }

#if $validatorIfdefName
        template<typename T>
        static void assertCorrectValue(JSONValue* value) {
            RuntimeCastHelper::assertType<JSONValue::TypeString>(value);
        }
#endif  // $validatorIfdefName
    };
};

template<>
struct ArrayItemHelper<int> {
    struct Traits {
        static void pushRaw(JSONArray* array, int value)
        {
            array->pushInt(value);
        }

#if $validatorIfdefName
        template<typename T>
        static void assertCorrectValue(JSONValue* value) {
            RuntimeCastHelper::assertInt(value);
        }
#endif  // $validatorIfdefName
    };
};

template<>
struct ArrayItemHelper<double> {
    struct Traits {
        static void pushRaw(JSONArray* array, double value)
        {
            array->pushNumber(value);
        }

#if $validatorIfdefName
        template<typename T>
        static void assertCorrectValue(JSONValue* value) {
            RuntimeCastHelper::assertType<JSONValue::TypeNumber>(value);
        }
#endif  // $validatorIfdefName
    };
};

template<>
struct ArrayItemHelper<bool> {
    struct Traits {
        static void pushRaw(JSONArray* array, bool value)
        {
            array->pushBoolean(value);
        }

#if $validatorIfdefName
        template<typename T>
        static void assertCorrectValue(JSONValue* value) {
            RuntimeCastHelper::assertType<JSONValue::TypeBoolean>(value);
        }
#endif  // $validatorIfdefName
    };
};

template<>
struct ArrayItemHelper<JSONValue> {
    struct Traits {
        static void pushRefPtr(JSONArray* array, PassRefPtr<JSONValue> value)
        {
            array->pushValue(value);
        }

#if $validatorIfdefName
        template<typename T>
        static void assertCorrectValue(JSONValue* value) {
            RuntimeCastHelper::assertAny(value);
        }
#endif  // $validatorIfdefName
    };
};

template<>
struct ArrayItemHelper<JSONObject> {
    struct Traits {
        static void pushRefPtr(JSONArray* array, PassRefPtr<JSONValue> value)
        {
            array->pushValue(value);
        }

#if $validatorIfdefName
        template<typename T>
        static void assertCorrectValue(JSONValue* value) {
            RuntimeCastHelper::assertType<JSONValue::TypeObject>(value);
        }
#endif  // $validatorIfdefName
    };
};

template<>
struct ArrayItemHelper<JSONArray> {
    struct Traits {
        static void pushRefPtr(JSONArray* array, PassRefPtr<JSONArray> value)
        {
            array->pushArray(value);
        }

#if $validatorIfdefName
        template<typename T>
        static void assertCorrectValue(JSONValue* value) {
            RuntimeCastHelper::assertType<JSONValue::TypeArray>(value);
        }
#endif  // $validatorIfdefName
    };
};

template<typename T>
struct ArrayItemHelper<protocol::TypeBuilder::Array<T>> {
    struct Traits {
        static void pushRefPtr(JSONArray* array, PassRefPtr<protocol::TypeBuilder::Array<T>> value)
        {
            array->pushValue(value);
        }

#if $validatorIfdefName
        template<typename S>
        static void assertCorrectValue(JSONValue* value) {
            S::assertCorrectValue(value);
        }
#endif  // $validatorIfdefName
    };
};

${forwards}

PLATFORM_EXPORT String getEnumConstantValue(int code);

${typeBuilders}
} // namespace TypeBuilder

} // namespace protocol
} // namespace blink

#endif // !defined(TypeBuilder_h)

""")

typebuilder_cpp = (
"""

#include "platform/inspector_protocol/TypeBuilder.h"
#include "wtf/text/CString.h"

namespace blink {
namespace protocol {

namespace TypeBuilder {

const char* const enum_constant_values[] = {
$enumConstantValues};

String getEnumConstantValue(int code) {
    return enum_constant_values[code];
}

} // namespace TypeBuilder

$implCode

#if $validatorIfdefName

void TypeBuilder::RuntimeCastHelper::assertAny(JSONValue*)
{
    // No-op.
}


void TypeBuilder::RuntimeCastHelper::assertInt(JSONValue* value)
{
    double v;
    bool castRes = value->asNumber(&v);
    ASSERT_UNUSED(castRes, castRes);
    ASSERT(static_cast<double>(static_cast<int>(v)) == v);
}

$validatorCode

#endif // $validatorIfdefName

} // namespace protocol
} // namespace blink

""")

param_container_access_code = """
    RefPtr<JSONObject> paramsContainer = requestMessageObject->getObject("params");
    JSONObject* paramsContainerPtr = paramsContainer.get();
"""

class_binding_builder_part_1 = (
"""        AllFieldsSet = %s
    };

    template<int STATE>
    class Builder {
    private:
        RefPtr<JSONObject> m_result;

        template<int STEP> Builder<STATE | STEP>& castState()
        {
            return *reinterpret_cast<Builder<STATE | STEP>*>(this);
        }

        Builder(PassRefPtr</*%s*/JSONObject> ptr)
        {
            static_assert(STATE == NoFieldsSet, "builder should not be created in non-init state");
            m_result = ptr;
        }
        friend class %s;
    public:
""")

class_binding_builder_part_2 = ("""
        Builder<STATE | %s>& set%s(%s value)
        {
            static_assert(!(STATE & %s), "property %s should not be set yet");
            m_result->set%s("%s", %s);
            return castState<%s>();
        }
""")

class_binding_builder_part_3 = ("""
        operator RefPtr<%s>& ()
        {
            static_assert(STATE == AllFieldsSet, "state should be AllFieldsSet");
            static_assert(sizeof(%s) == sizeof(JSONObject), "%s should be the same size as JSONObject");
            return *reinterpret_cast<RefPtr<%s>*>(&m_result);
        }

        PassRefPtr<%s> release()
        {
            return RefPtr<%s>(*this).release();
        }
    };

""")

class_binding_builder_part_4 = (
"""    static Builder<NoFieldsSet> create()
    {
        return Builder<NoFieldsSet>(JSONObject::create());
    }
""")
