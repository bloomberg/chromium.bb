/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "webkit/tools/pepper_test_plugin/test_object.h"

#include <stdlib.h>

#include "webkit/tools/pepper_test_plugin/plugin_object.h"

static bool testEnumerate(NPObject*, NPIdentifier **value, uint32_t *count);
static bool testHasMethod(NPObject*, NPIdentifier name);
static bool testInvoke(NPObject* header, NPIdentifier name, const NPVariant* args, uint32_t argCount, NPVariant* result);
static bool testInvokeDefault(NPObject*, const NPVariant *args, uint32_t argCount, NPVariant *result);
static bool testHasProperty(NPObject*, NPIdentifier name);
static bool testGetProperty(NPObject*, NPIdentifier name, NPVariant *variant);
static NPObject *testAllocate(NPP npp, NPClass *theClass);
static void testDeallocate(NPObject*);
static bool testConstruct(NPObject*, const NPVariant* args, uint32_t argCount, NPVariant* result);

static NPClass testClass = {
    NP_CLASS_STRUCT_VERSION,
    testAllocate,
    testDeallocate,
    0,
    testHasMethod,
    testInvoke,
    testInvokeDefault,
    testHasProperty,
    testGetProperty,
    0,
    0,
    testEnumerate,
    testConstruct
};

NPClass* GetTestClass()
{
    return &testClass;
}

int testObjectCount = 0;

int GetTestObjectCount()
{
  return testObjectCount;
}

static bool identifiersInitialized = false;

#define NUM_ENUMERABLE_TEST_IDENTIFIERS 4
#define NUM_TEST_IDENTIFIERS            5

#define ID_PROPERTY_FOO            0
#define ID_PROPERTY_BAR            1
#define ID_PROPERTY_TEST_OBJECT    2
#define ID_PROPERTY_REF_COUNT      3
#define ID_PROPERTY_OBJECT_POINTER 4

static NPIdentifier testIdentifiers[NUM_TEST_IDENTIFIERS];
static const NPUTF8 *testIdentifierNames[NUM_TEST_IDENTIFIERS] = {
    "foo",
    "bar",
    "testObject",
    "refCount",
    "objectPointer",
};

#define ID_THROW_EXCEPTION_METHOD  0
#define NUM_METHOD_IDENTIFIERS     1

static NPIdentifier testMethodIdentifiers[NUM_METHOD_IDENTIFIERS];
static const NPUTF8 *testMethodIdentifierNames[NUM_METHOD_IDENTIFIERS] = {
    "throwException",
};

static void initializeIdentifiers(void)
{
    browser->getstringidentifiers(testIdentifierNames, NUM_TEST_IDENTIFIERS, testIdentifiers);
    browser->getstringidentifiers(testMethodIdentifierNames, NUM_METHOD_IDENTIFIERS, testMethodIdentifiers);
}

static NPObject *testAllocate(NPP npp, NPClass *theClass)
{
    TestObject *newInstance =
        static_cast<TestObject*>(malloc(sizeof(TestObject)));
    newInstance->test_object = NULL;
    ++testObjectCount;

    if (!identifiersInitialized) {
        identifiersInitialized = true;
        initializeIdentifiers();
    }

    return reinterpret_cast<NPObject*>(newInstance);
}

static void testDeallocate(NPObject *obj)
{
    TestObject *testObject = reinterpret_cast<TestObject*>(obj);
    if (testObject->test_object)
      browser->releaseobject(testObject->test_object);
    --testObjectCount;
    free(obj);
}

static bool testHasMethod(NPObject*, NPIdentifier name)
{
    for (unsigned i = 0; i < NUM_METHOD_IDENTIFIERS; i++) {
        if (testMethodIdentifiers[i] == name)
            return true;
    }
    return false;
}

static bool testInvoke(NPObject* header, NPIdentifier name, const NPVariant* /*args*/, uint32_t /*argCount*/, NPVariant* /*result*/)
{
    if (name == testMethodIdentifiers[ID_THROW_EXCEPTION_METHOD]) {
        browser->setexception(header, "test object throwException SUCCESS");
        return true;
    }
    return false;
}

static bool testInvokeDefault(NPObject *obj, const NPVariant *args,
                              uint32_t argCount, NPVariant *result)
{
    INT32_TO_NPVARIANT(2, *result);
    return true;
}

static bool testHasProperty(NPObject*, NPIdentifier name)
{
    for (unsigned i = 0; i < NUM_TEST_IDENTIFIERS; i++) {
        if (testIdentifiers[i] == name)
            return true;
    }

    return false;
}

static bool testGetProperty(NPObject *obj, NPIdentifier name,
                            NPVariant *variant)
{
    if (name == testIdentifiers[ID_PROPERTY_FOO]) {
        char* mem = static_cast<char*>(browser->memalloc(4));
        strcpy(mem, "foo");
        STRINGZ_TO_NPVARIANT(mem, *variant);
        return true;
    } else if (name == testIdentifiers[ID_PROPERTY_BAR]) {
        BOOLEAN_TO_NPVARIANT(true, *variant);
        return true;
    } else if (name == testIdentifiers[ID_PROPERTY_TEST_OBJECT]) {
        TestObject* testObject = reinterpret_cast<TestObject*>(obj);
        if (testObject->test_object == NULL)
          testObject->test_object = browser->createobject(NULL, &testClass);
        browser->retainobject(testObject->test_object);
        OBJECT_TO_NPVARIANT(testObject->test_object, *variant);
        return true;
    } else if (name == testIdentifiers[ID_PROPERTY_REF_COUNT]) {
        INT32_TO_NPVARIANT(obj->referenceCount, *variant);
        return true;
    } else if (name == testIdentifiers[ID_PROPERTY_OBJECT_POINTER]) {
        int32_t objectPointer = static_cast<int32_t>(reinterpret_cast<long long>(obj));
        INT32_TO_NPVARIANT(objectPointer, *variant);
        return true;
    }
    return false;
}

static bool testEnumerate(NPObject *npobj, NPIdentifier **value, uint32_t *count)
{
    *count = NUM_ENUMERABLE_TEST_IDENTIFIERS;

    *value = (NPIdentifier*)browser->memalloc(NUM_ENUMERABLE_TEST_IDENTIFIERS * sizeof(NPIdentifier));
    memcpy(*value, testIdentifiers, sizeof(NPIdentifier) * NUM_ENUMERABLE_TEST_IDENTIFIERS);

    return true;
}

static bool testConstruct(NPObject* npobj, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    browser->retainobject(npobj);

    // Just return the same object.
    OBJECT_TO_NPVARIANT(npobj, *result);
    return true;
}


