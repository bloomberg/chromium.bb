// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/Headers.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/xml/XMLHttpRequest.h"
#include "modules/serviceworkers/HeadersForEachCallback.h"
#include "wtf/NotFound.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

PassRefPtrWillBeRawPtr<Headers> Headers::create()
{
    return adoptRefWillBeNoop(new Headers);
}

PassRefPtrWillBeRawPtr<Headers> Headers::create(ExceptionState&)
{
    return create();
}

PassRefPtrWillBeRawPtr<Headers> Headers::create(const Headers* init, ExceptionState& exceptionState)
{
    // "The Headers(|init|) constructor, when invoked, must run these steps:"
    // "1. Let |headers| be a new Headers object."
    RefPtrWillBeRawPtr<Headers> headers = create();
    // "2. If |init| is given, fill headers with |init|. Rethrow any exception."
    headers->fillWith(init, exceptionState);
    // "3. Return |headers|."
    return headers.release();
}

PassRefPtrWillBeRawPtr<Headers> Headers::create(const Dictionary& init, ExceptionState& exceptionState)
{
    // "The Headers(|init|) constructor, when invoked, must run these steps:"
    // "1. Let |headers| be a new Headers object."
    RefPtrWillBeRawPtr<Headers> headers = create();
    // "2. If |init| is given, fill headers with |init|. Rethrow any exception."
    headers->fillWith(init, exceptionState);
    // "3. Return |headers|."
    return headers.release();
}

PassRefPtrWillBeRawPtr<Headers> Headers::create(FetchHeaderList* headerList)
{
    return adoptRefWillBeNoop(new Headers(headerList));
}

PassRefPtrWillBeRawPtr<Headers> Headers::createCopy() const
{
    RefPtrWillBeRawPtr<FetchHeaderList> headerList = m_headerList->createCopy();
    RefPtrWillBeRawPtr<Headers> headers = create(headerList.get());
    headers->m_guard = m_guard;
    return headers.release();
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(Headers);

unsigned long Headers::size() const
{
    return m_headerList->size();
}

void Headers::append(const String& name, const String& value, ExceptionState& exceptionState)
{
    // "To append a name/value (|name|/|value|) pair to a Headers object
    // (|headers|), run these steps:"
    // "1. If |name| is not a name or |value| is not a value, throw a
    //     TypeError."
    if (!FetchHeaderList::isValidHeaderName(name)) {
        exceptionState.throwTypeError("Invalid name");
        return;
    }
    if (!FetchHeaderList::isValidHeaderValue(value)) {
        exceptionState.throwTypeError("Invalid value");
        return;
    }
    // "2. If guard is |request|, throw a TypeError."
    if (m_guard == ImmutableGuard) {
        exceptionState.throwTypeError("Headers are immutable");
        return;
    }
    // "3. Otherwise, if guard is |request| and |name| is a forbidden header
    //     name, return."
    if (m_guard == RequestGuard && FetchHeaderList::isForbiddenHeaderName(name))
        return;
    // "4. Otherwise, if guard is |request-no-CORS| and |name|/|value| is not a
    //     simple header, return."
    if (m_guard == RequestNoCORSGuard && !FetchHeaderList::isSimpleHeader(name, value))
        return;
    // "5. Otherwise, if guard is |response| and |name| is a forbidden response
    //     header name, return."
    if (m_guard == ResponseGuard && FetchHeaderList::isForbiddenResponseHeaderName(name))
        return;
    // "6. Append |name|/|value| to header list."
    m_headerList->append(name, value);
}

void Headers::remove(const String& name, ExceptionState& exceptionState)
{
    // "The delete(|name|) method, when invoked, must run these steps:"
    // "1. If name is not a name, throw a TypeError."
    if (!FetchHeaderList::isValidHeaderName(name)) {
        exceptionState.throwTypeError("Invalid name");
        return;
    }
    // "2. If guard is |immutable|, throw a TypeError."
    if (m_guard == ImmutableGuard) {
        exceptionState.throwTypeError("Headers are immutable");
        return;
    }
    // "3. Otherwise, if guard is |request| and |name| is a forbidden header
    //     name, return."
    if (m_guard == RequestGuard && FetchHeaderList::isForbiddenHeaderName(name))
        return;
    // "4. Otherwise, if guard is |request-no-CORS| and |name|/`invalid` is not
    //     a simple header, return."
    if (m_guard == RequestNoCORSGuard && !FetchHeaderList::isSimpleHeader(name, "invalid"))
        return;
    // "5. Otherwise, if guard is |response| and |name| is a forbidden response
    //     header name, return."
    if (m_guard == ResponseGuard && FetchHeaderList::isForbiddenResponseHeaderName(name))
        return;
    // "6. Delete |name| from header list."
    m_headerList->remove(name);
}

String Headers::get(const String& name, ExceptionState& exceptionState)
{
    // "The get(|name|) method, when invoked, must run these steps:"
    // "1. If |name| is not a name, throw a TypeError."
    if (!FetchHeaderList::isValidHeaderName(name)) {
        exceptionState.throwTypeError("Invalid name");
        return String();
    }
    // "2. Return the value of the first header in header list whose name is
    //     |name|, and null otherwise."
    String result;
    m_headerList->get(name, result);
    return result;
}

Vector<String> Headers::getAll(const String& name, ExceptionState& exceptionState)
{
    // "The getAll(|name|) method, when invoked, must run these steps:"
    // "1. If |name| is not a name, throw a TypeError."
    if (!FetchHeaderList::isValidHeaderName(name)) {
        exceptionState.throwTypeError("Invalid name");
        return Vector<String>();
    }
    // "2. Return the values of all headers in header list whose name is |name|,
    //     in list order, and the empty sequence otherwise."
    Vector<String> result;
    m_headerList->getAll(name, result);
    return result;
}

bool Headers::has(const String& name, ExceptionState& exceptionState)
{
    // "The has(|name|) method, when invoked, must run these steps:"
    // "1. If |name| is not a name, throw a TypeError."
    if (!FetchHeaderList::isValidHeaderName(name)) {
        exceptionState.throwTypeError("Invalid name");
        return false;
    }
    // "2. Return true if there is a header in header list whose name is |name|,
    //     and false otherwise."
    return m_headerList->has(name);
}

void Headers::set(const String& name, const String& value, ExceptionState& exceptionState)
{
    // "The set(|name|, |value|) method, when invoked, must run these steps:"
    // "1. If |name| is not a name or |value| is not a value, throw a
    //     TypeError."
    if (!FetchHeaderList::isValidHeaderName(name)) {
        exceptionState.throwTypeError("Invalid name");
        return;
    }
    if (!FetchHeaderList::isValidHeaderValue(value)) {
        exceptionState.throwTypeError("Invalid value");
        return;
    }
    // "2. If guard is |immutable|, throw a TypeError."
    if (m_guard == ImmutableGuard) {
        exceptionState.throwTypeError("Headers are immutable");
        return;
    }
    // "3. Otherwise, if guard is |request| and |name| is a forbidden header
    //     name, return."
    if (m_guard == RequestGuard && FetchHeaderList::isForbiddenHeaderName(name))
        return;
    // "4. Otherwise, if guard is |request-no-CORS| and |name|/|value| is not a
    //     simple header, return."
    if (m_guard == RequestNoCORSGuard && !FetchHeaderList::isSimpleHeader(name, value))
        return;
    // "5. Otherwise, if guard is |response| and |name| is a forbidden response
    //     header name, return."
    if (m_guard == ResponseGuard && FetchHeaderList::isForbiddenResponseHeaderName(name))
        return;
    // "6. Set |name|/|value| in header list."
    m_headerList->set(name, value);
}

void Headers::forEach(PassOwnPtr<HeadersForEachCallback> callback, ScriptValue& thisArg)
{
    forEachInternal(callback, &thisArg);
}

void Headers::forEach(PassOwnPtr<HeadersForEachCallback> callback)
{
    forEachInternal(callback, 0);
}

void Headers::fillWith(const Headers* object, ExceptionState& exceptionState)
{
    ASSERT(m_headerList->size() == 0);
    // "To fill a Headers object (|this|) with a given object (|object|), run
    // these steps:"
    // "1. If |object| is a Headers object, copy its header list as
    //     |headerListCopy| and then for each |header| in |headerListCopy|,
    //     retaining order, append header's |name|/|header|'s value to
    //     |headers|. Rethrow any exception."
    for (size_t i = 0; i < object->m_headerList->list().size(); ++i) {
        append(object->m_headerList->list()[i]->first, object->m_headerList->list()[i]->second, exceptionState);
        if (exceptionState.hadException())
            return;
    }
}

void Headers::fillWith(const Dictionary& object, ExceptionState& exceptionState)
{
    ASSERT(m_headerList->size() == 0);
    Vector<String> keys;
    object.getOwnPropertyNames(keys);
    if (keys.size() == 0)
        return;

    // Because of the restrictions in IDL compiler of blink we recieve
    // sequence<sequence<ByteString>> as a Dictionary, which is a type of union
    // type of HeadersInit defined in the spec.
    // http://fetch.spec.whatwg.org/#headers-class
    // FIXME: Support sequence<sequence<ByteString>>.
    Vector<String> keyValuePair;
    if (DictionaryHelper::get(object, keys[0], keyValuePair)) {
        // "2. Otherwise, if |object| is a sequence, then for each |header| in
        //     |object|, run these substeps:
        //    1. If |header| does not contain exactly two items, throw a
        //       TypeError.
        //    2. Append |header|'s first item/|header|'s second item to
        //       |headers|. Rethrow any exception."
        for (size_t i = 0; i < keys.size(); ++i) {
            // We've already got the keyValuePair for key[0].
            if (i > 0) {
                if (!DictionaryHelper::get(object, keys[i], keyValuePair)) {
                    exceptionState.throwTypeError("Invalid value");
                    return;
                }
            }
            if (keyValuePair.size() != 2) {
                exceptionState.throwTypeError("Invalid value");
                return;
            }
            append(keyValuePair[0], keyValuePair[1], exceptionState);
            if (exceptionState.hadException())
                return;
            keyValuePair.clear();
        }
        return;
    }
    // "3. Otherwise, if |object| is an open-ended dictionary, then for each
    //    |header| in object, run these substeps:
    //    1. Set |header|'s key to |header|'s key, converted to ByteString.
    //       Rethrow any exception.
    //    2. Append |header|'s key/|header|'s value to |headers|. Rethrow any
    //       exception."
    // FIXME: Support OpenEndedDictionary<ByteString>.
    for (size_t i = 0; i < keys.size(); ++i) {
        String value;
        if (!DictionaryHelper::get(object, keys[i], value)) {
            exceptionState.throwTypeError("Invalid value");
            return;
        }
        append(keys[i], value, exceptionState);
        if (exceptionState.hadException())
            return;
    }
}

Headers::Headers()
    : m_headerList(FetchHeaderList::create())
    , m_guard(NoneGuard)
{
    ScriptWrappable::init(this);
}

Headers::Headers(FetchHeaderList* headerList)
    : m_headerList(headerList)
    , m_guard(NoneGuard)
{
    ScriptWrappable::init(this);
}

void Headers::forEachInternal(PassOwnPtr<HeadersForEachCallback> callback, ScriptValue* thisArg)
{
    TrackExceptionState exceptionState;
    for (size_t i = 0; i < m_headerList->size(); ++i) {
        if (thisArg)
            callback->handleItem(*thisArg, m_headerList->list()[i]->second, m_headerList->list()[i]->first, this);
        else
            callback->handleItem(m_headerList->list()[i]->second, m_headerList->list()[i]->first, this);
        if (exceptionState.hadException())
            break;
    }
}

void Headers::trace(Visitor* visitor)
{
    visitor->trace(m_headerList);
}

} // namespace blink
