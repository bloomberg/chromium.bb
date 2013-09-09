/*
* Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*     * Neither the name of Google Inc. nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
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

#include "config.h"

#include "bindings/v8/V8NPObject.h"

#include "V8HTMLAppletElement.h"
#include "V8HTMLEmbedElement.h"
#include "V8HTMLObjectElement.h"
#include "bindings/v8/NPV8Object.h"
#include "bindings/v8/UnsafePersistent.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8NPUtils.h"
#include "bindings/v8/V8ObjectConstructor.h"
#include "bindings/v8/npruntime_impl.h"
#include "bindings/v8/npruntime_priv.h"
#include "core/html/HTMLPlugInElement.h"
#include "wtf/OwnArrayPtr.h"

namespace WebCore {

enum InvokeFunctionType {
    InvokeMethod = 1,
    InvokeConstruct = 2,
    InvokeDefault = 3
};

struct IdentifierRep {
    int number() const { return m_isString ? 0 : m_value.m_number; }
    const char* string() const { return m_isString ? m_value.m_string : 0; }

    union {
        const char* m_string;
        int m_number;
    } m_value;
    bool m_isString;
};

// FIXME: need comments.
// Params: holder could be HTMLEmbedElement or NPObject
static void npObjectInvokeImpl(const v8::FunctionCallbackInfo<v8::Value>& args, InvokeFunctionType functionId)
{
    NPObject* npObject;

    WrapperWorldType currentWorldType = worldType(args.GetIsolate());
    // These three types are subtypes of HTMLPlugInElement.
    if (V8HTMLAppletElement::HasInstance(args.Holder(), args.GetIsolate(), currentWorldType) || V8HTMLEmbedElement::HasInstance(args.Holder(), args.GetIsolate(), currentWorldType)
        || V8HTMLObjectElement::HasInstance(args.Holder(), args.GetIsolate(), currentWorldType)) {
        // The holder object is a subtype of HTMLPlugInElement.
        HTMLPlugInElement* element;
        if (V8HTMLAppletElement::HasInstance(args.Holder(), args.GetIsolate(), currentWorldType))
            element = V8HTMLAppletElement::toNative(args.Holder());
        else if (V8HTMLEmbedElement::HasInstance(args.Holder(), args.GetIsolate(), currentWorldType))
            element = V8HTMLEmbedElement::toNative(args.Holder());
        else
            element = V8HTMLObjectElement::toNative(args.Holder());
        ScriptInstance scriptInstance = element->getInstance();
        if (scriptInstance) {
            v8::Isolate* isolate = v8::Isolate::GetCurrent();
            v8::HandleScope handleScope(isolate);
            npObject = v8ObjectToNPObject(scriptInstance->newLocal(isolate));
        } else
            npObject = 0;
    } else {
        // The holder object is not a subtype of HTMLPlugInElement, it must be an NPObject which has three
        // internal fields.
        if (args.Holder()->InternalFieldCount() != npObjectInternalFieldCount) {
            throwError(v8ReferenceError, "NPMethod called on non-NPObject", args.GetIsolate());
            return;
        }

        npObject = v8ObjectToNPObject(args.Holder());
    }

    // Verify that our wrapper wasn't using a NPObject which has already been deleted.
    if (!npObject || !_NPN_IsAlive(npObject)) {
        throwError(v8ReferenceError, "NPObject deleted", args.GetIsolate());
        return;
    }

    // Wrap up parameters.
    int numArgs = args.Length();
    OwnArrayPtr<NPVariant> npArgs = adoptArrayPtr(new NPVariant[numArgs]);

    for (int i = 0; i < numArgs; i++)
        convertV8ObjectToNPVariant(args[i], npObject, &npArgs[i]);

    NPVariant result;
    VOID_TO_NPVARIANT(result);

    bool retval = true;
    switch (functionId) {
    case InvokeMethod:
        if (npObject->_class->invoke) {
            v8::Handle<v8::String> functionName = v8::Handle<v8::String>::Cast(args.Data());
            NPIdentifier identifier = getStringIdentifier(functionName);
            retval = npObject->_class->invoke(npObject, identifier, npArgs.get(), numArgs, &result);
        }
        break;
    case InvokeConstruct:
        if (npObject->_class->construct)
            retval = npObject->_class->construct(npObject, npArgs.get(), numArgs, &result);
        break;
    case InvokeDefault:
        if (npObject->_class->invokeDefault)
            retval = npObject->_class->invokeDefault(npObject, npArgs.get(), numArgs, &result);
        break;
    default:
        break;
    }

    if (!retval)
        throwError(v8GeneralError, "Error calling method on NPObject.", args.GetIsolate());

    for (int i = 0; i < numArgs; i++)
        _NPN_ReleaseVariantValue(&npArgs[i]);

    // Unwrap return values.
    v8::Handle<v8::Value> returnValue;
    if (_NPN_IsAlive(npObject))
        returnValue = convertNPVariantToV8Object(&result, npObject, args.GetIsolate());
    _NPN_ReleaseVariantValue(&result);

    v8SetReturnValue(args, returnValue);
}


void npObjectMethodHandler(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    return npObjectInvokeImpl(args, InvokeMethod);
}


void npObjectInvokeDefaultHandler(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.IsConstructCall()) {
        npObjectInvokeImpl(args, InvokeConstruct);
        return;
    }

    npObjectInvokeImpl(args, InvokeDefault);
}

class V8NPTemplateMap {
public:
    // NPIdentifier is PrivateIdentifier*.
    typedef HashMap<PrivateIdentifier*, UnsafePersistent<v8::FunctionTemplate> > MapType;

    UnsafePersistent<v8::FunctionTemplate> get(PrivateIdentifier* key)
    {
        return m_map.get(key);
    }

    void set(PrivateIdentifier* key, v8::Handle<v8::FunctionTemplate> handle)
    {
        ASSERT(!m_map.contains(key));
        v8::Persistent<v8::FunctionTemplate> wrapper(m_isolate, handle);
        wrapper.MakeWeak(key, &makeWeakCallback);
        m_map.set(key, UnsafePersistent<v8::FunctionTemplate>(wrapper));
    }

    static V8NPTemplateMap& sharedInstance(v8::Isolate* isolate)
    {
        DEFINE_STATIC_LOCAL(V8NPTemplateMap, map, (isolate));
        ASSERT(isolate == map.m_isolate);
        return map;
    }

private:
    explicit V8NPTemplateMap(v8::Isolate* isolate)
        : m_isolate(isolate)
    {
    }

    void dispose(PrivateIdentifier* key)
    {
        MapType::iterator it = m_map.find(key);
        ASSERT(it != m_map.end());
        it->value.dispose();
        m_map.remove(it);
    }

    static void makeWeakCallback(v8::Isolate* isolate, v8::Persistent<v8::FunctionTemplate>*, PrivateIdentifier* key)
    {
        V8NPTemplateMap::sharedInstance(isolate).dispose(key);
    }

    MapType m_map;
    v8::Isolate* m_isolate;
};

static v8::Handle<v8::Value> npObjectGetProperty(v8::Local<v8::Object> self, NPIdentifier identifier, v8::Local<v8::Value> key, v8::Isolate* isolate)
{
    NPObject* npObject = v8ObjectToNPObject(self);

    // Verify that our wrapper wasn't using a NPObject which
    // has already been deleted.
    if (!npObject || !_NPN_IsAlive(npObject))
        return throwError(v8ReferenceError, "NPObject deleted", isolate);


    if (npObject->_class->hasProperty && npObject->_class->getProperty && npObject->_class->hasProperty(npObject, identifier)) {
        if (!_NPN_IsAlive(npObject))
            return throwError(v8ReferenceError, "NPObject deleted", isolate);

        NPVariant result;
        VOID_TO_NPVARIANT(result);
        if (!npObject->_class->getProperty(npObject, identifier, &result))
            return v8Undefined();

        v8::Handle<v8::Value> returnValue;
        if (_NPN_IsAlive(npObject))
            returnValue = convertNPVariantToV8Object(&result, npObject, isolate);
        _NPN_ReleaseVariantValue(&result);
        return returnValue;

    }

    if (!_NPN_IsAlive(npObject))
        return throwError(v8ReferenceError, "NPObject deleted", isolate);

    if (key->IsString() && npObject->_class->hasMethod && npObject->_class->hasMethod(npObject, identifier)) {
        if (!_NPN_IsAlive(npObject))
            return throwError(v8ReferenceError, "NPObject deleted", isolate);

        PrivateIdentifier* id = static_cast<PrivateIdentifier*>(identifier);
        UnsafePersistent<v8::FunctionTemplate> functionTemplate = V8NPTemplateMap::sharedInstance(isolate).get(id);
        // FunctionTemplate caches function for each context.
        v8::Local<v8::Function> v8Function;
        // Cache templates using identifier as the key.
        if (functionTemplate.isEmpty()) {
            // Create a new template.
            v8::Local<v8::FunctionTemplate> temp = v8::FunctionTemplate::New();
            temp->SetCallHandler(npObjectMethodHandler, key);
            V8NPTemplateMap::sharedInstance(isolate).set(id, temp);
            v8Function = temp->GetFunction();
        } else {
            v8Function = functionTemplate.newLocal(isolate)->GetFunction();
        }
        v8Function->SetName(v8::Handle<v8::String>::Cast(key));
        return v8Function;
    }

    return v8Undefined();
}

void npObjectNamedPropertyGetter(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    NPIdentifier identifier = getStringIdentifier(name);
    v8SetReturnValue(info, npObjectGetProperty(info.Holder(), identifier, name, info.GetIsolate()));
}

void npObjectIndexedPropertyGetter(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    NPIdentifier identifier = _NPN_GetIntIdentifier(index);
    v8SetReturnValue(info, npObjectGetProperty(info.Holder(), identifier, v8::Number::New(index), info.GetIsolate()));
}

void npObjectGetNamedProperty(v8::Local<v8::Object> self, v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    NPIdentifier identifier = getStringIdentifier(name);
    v8SetReturnValue(info, npObjectGetProperty(self, identifier, name, info.GetIsolate()));
}

void npObjectGetIndexedProperty(v8::Local<v8::Object> self, uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    NPIdentifier identifier = _NPN_GetIntIdentifier(index);
    v8SetReturnValue(info, npObjectGetProperty(self, identifier, v8::Number::New(index), info.GetIsolate()));
}

void npObjectQueryProperty(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Integer>& info)
{
    NPIdentifier identifier = getStringIdentifier(name);
    if (npObjectGetProperty(info.Holder(), identifier, name, info.GetIsolate()).IsEmpty())
        return;
    v8SetReturnValueInt(info, 0);
}

static v8::Handle<v8::Value> npObjectSetProperty(v8::Local<v8::Object> self, NPIdentifier identifier, v8::Local<v8::Value> value, v8::Isolate* isolate)
{
    NPObject* npObject = v8ObjectToNPObject(self);

    // Verify that our wrapper wasn't using a NPObject which has already been deleted.
    if (!npObject || !_NPN_IsAlive(npObject)) {
        throwError(v8ReferenceError, "NPObject deleted", isolate);
        return value;  // Intercepted, but an exception was thrown.
    }

    if (npObject->_class->hasProperty && npObject->_class->setProperty && npObject->_class->hasProperty(npObject, identifier)) {
        if (!_NPN_IsAlive(npObject))
            return throwError(v8ReferenceError, "NPObject deleted", isolate);

        NPVariant npValue;
        VOID_TO_NPVARIANT(npValue);
        convertV8ObjectToNPVariant(value, npObject, &npValue);
        bool success = npObject->_class->setProperty(npObject, identifier, &npValue);
        _NPN_ReleaseVariantValue(&npValue);
        if (success)
            return value; // Intercept the call.
    }
    return v8Undefined();
}


void npObjectNamedPropertySetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    NPIdentifier identifier = getStringIdentifier(name);
    v8SetReturnValue(info, npObjectSetProperty(info.Holder(), identifier, value, info.GetIsolate()));
}


void npObjectIndexedPropertySetter(uint32_t index, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    NPIdentifier identifier = _NPN_GetIntIdentifier(index);
    v8SetReturnValue(info, npObjectSetProperty(info.Holder(), identifier, value, info.GetIsolate()));
}

void npObjectSetNamedProperty(v8::Local<v8::Object> self, v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    NPIdentifier identifier = getStringIdentifier(name);
    v8SetReturnValue(info, npObjectSetProperty(self, identifier, value, info.GetIsolate()));
}

void npObjectSetIndexedProperty(v8::Local<v8::Object> self, uint32_t index, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    NPIdentifier identifier = _NPN_GetIntIdentifier(index);
    v8SetReturnValue(info, npObjectSetProperty(self, identifier, value, info.GetIsolate()));
}

void npObjectPropertyEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info, bool namedProperty)
{
    NPObject* npObject = v8ObjectToNPObject(info.Holder());

    // Verify that our wrapper wasn't using a NPObject which
    // has already been deleted.
    if (!npObject || !_NPN_IsAlive(npObject))
        throwError(v8ReferenceError, "NPObject deleted", info.GetIsolate());

    if (NP_CLASS_STRUCT_VERSION_HAS_ENUM(npObject->_class) && npObject->_class->enumerate) {
        uint32_t count;
        NPIdentifier* identifiers;
        if (npObject->_class->enumerate(npObject, &identifiers, &count)) {
            v8::Handle<v8::Array> properties = v8::Array::New(count);
            for (uint32_t i = 0; i < count; ++i) {
                IdentifierRep* identifier = static_cast<IdentifierRep*>(identifiers[i]);
                if (namedProperty)
                    properties->Set(v8::Integer::New(i, info.GetIsolate()), v8::String::NewSymbol(identifier->string()));
                else
                    properties->Set(v8::Integer::New(i, info.GetIsolate()), v8::Integer::New(identifier->number(), info.GetIsolate()));
            }

            v8SetReturnValue(info, properties);
            return;
        }
    }
}

void npObjectNamedPropertyEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info)
{
    npObjectPropertyEnumerator(info, true);
}

void npObjectIndexedPropertyEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info)
{
    npObjectPropertyEnumerator(info, false);
}

static DOMWrapperMap<NPObject>& staticNPObjectMap()
{
    DEFINE_STATIC_LOCAL(DOMWrapperMap<NPObject>, npObjectMap, (v8::Isolate::GetCurrent()));
    return npObjectMap;
}

template<>
inline void DOMWrapperMap<NPObject>::makeWeakCallback(v8::Isolate* isolate, v8::Persistent<v8::Object>* wrapper, DOMWrapperMap<NPObject>*)
{
    NPObject* npObject = static_cast<NPObject*>(toNative(*wrapper));

    ASSERT(npObject);
    ASSERT(staticNPObjectMap().containsKeyAndValue(npObject, *wrapper));

    // Must remove from our map before calling _NPN_ReleaseObject(). _NPN_ReleaseObject can
    // call forgetV8ObjectForNPObject, which uses the table as well.
    staticNPObjectMap().removeAndDispose(npObject);

    if (_NPN_IsAlive(npObject))
        _NPN_ReleaseObject(npObject);
}

v8::Local<v8::Object> createV8ObjectForNPObject(NPObject* object, NPObject* root)
{
    static v8::Eternal<v8::FunctionTemplate> npObjectDesc;

    ASSERT(v8::Context::InContext());

    v8::Isolate* isolate = v8::Isolate::GetCurrent();

    // If this is a v8 object, just return it.
    V8NPObject* v8NPObject = npObjectToV8NPObject(object);
    if (v8NPObject)
        return v8::Local<v8::Object>::New(isolate, v8NPObject->v8Object);

    // If we've already wrapped this object, just return it.
    v8::Handle<v8::Object> wrapper = staticNPObjectMap().newLocal(object, isolate);
    if (!wrapper.IsEmpty())
        return wrapper;

    // FIXME: we should create a Wrapper type as a subclass of JSObject. It has two internal fields, field 0 is the wrapped
    // pointer, and field 1 is the type. There should be an api function that returns unused type id. The same Wrapper type
    // can be used by DOM bindings.
    if (npObjectDesc.IsEmpty()) {
        v8::Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New();
        templ->InstanceTemplate()->SetInternalFieldCount(npObjectInternalFieldCount);
        templ->InstanceTemplate()->SetNamedPropertyHandler(npObjectNamedPropertyGetter, npObjectNamedPropertySetter, npObjectQueryProperty, 0, npObjectNamedPropertyEnumerator);
        templ->InstanceTemplate()->SetIndexedPropertyHandler(npObjectIndexedPropertyGetter, npObjectIndexedPropertySetter, 0, 0, npObjectIndexedPropertyEnumerator);
        templ->InstanceTemplate()->SetCallAsFunctionHandler(npObjectInvokeDefaultHandler);
        npObjectDesc.Set(isolate, templ);
    }

    // FIXME: Move staticNPObjectMap() to DOMDataStore.
    // Use V8DOMWrapper::createWrapper() and
    // V8DOMWrapper::associateObjectWithWrapper()
    // to create a wrapper object.
    v8::Handle<v8::Function> v8Function = npObjectDesc.Get(isolate)->GetFunction();
    v8::Local<v8::Object> value = V8ObjectConstructor::newInstance(v8Function);
    if (value.IsEmpty())
        return value;

    V8DOMWrapper::setNativeInfo(value, npObjectTypeInfo(), object);

    // KJS retains the object as part of its wrapper (see Bindings::CInstance).
    _NPN_RetainObject(object);
    _NPN_RegisterObject(object, root);

    WrapperConfiguration configuration = buildWrapperConfiguration(object, WrapperConfiguration::Dependent);
    staticNPObjectMap().set(object, value, configuration);
    ASSERT(V8DOMWrapper::maybeDOMWrapper(value));
    return value;
}

void forgetV8ObjectForNPObject(NPObject* object)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Handle<v8::Object> wrapper = staticNPObjectMap().newLocal(object, isolate);
    if (!wrapper.IsEmpty()) {
        V8DOMWrapper::clearNativeInfo(wrapper, npObjectTypeInfo());
        staticNPObjectMap().removeAndDispose(object);
        _NPN_ReleaseObject(object);
    }
}

} // namespace WebCore
