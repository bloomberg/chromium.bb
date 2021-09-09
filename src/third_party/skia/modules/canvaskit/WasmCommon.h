/*
 * Copyright 2019 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef WasmCommon_DEFINED
#define WasmCommon_DEFINED

#include <emscripten.h>
#include <emscripten/bind.h>
#include "include/core/SkColor.h"
#include "include/core/SkSpan.h"

using namespace emscripten;

// Self-documenting types
using JSArray = emscripten::val;
using JSObject = emscripten::val;
using JSString = emscripten::val;
using SkPathOrNull = emscripten::val;
using TypedArray = emscripten::val;
using Uint8Array = emscripten::val;
using Uint16Array = emscripten::val;
using Uint32Array = emscripten::val;
using Float32Array = emscripten::val;

#define SPECIALIZE_JSARRAYTYPE(type, name)                  \
    template <> struct JSArrayType<type> {                  \
        static constexpr const char* const gName = name;    \
    }

template <typename T> struct JSArrayType {};

SPECIALIZE_JSARRAYTYPE( int8_t,    "Int8Array");
SPECIALIZE_JSARRAYTYPE(uint8_t,   "Uint8Array");
SPECIALIZE_JSARRAYTYPE( int16_t,  "Int16Array");
SPECIALIZE_JSARRAYTYPE(uint16_t, "Uint16Array");
SPECIALIZE_JSARRAYTYPE( int32_t,  "Int32Array");
SPECIALIZE_JSARRAYTYPE(uint32_t, "Uint32Array");
SPECIALIZE_JSARRAYTYPE(float,   "Float32Array");

#undef SPECIALIZE_JSARRAYTYPE

/**
 *  Create a typed-array (in the JS heap) and initialize it with the provided
 *  data (from the wasm heap).
 */
template <typename T> TypedArray MakeTypedArray(int count, const T src[]) {
    emscripten::val length = emscripten::val(count);
    emscripten::val jarray = emscripten::val::global(JSArrayType<T>::gName).new_(count);
    jarray.call<void>("set", val(typed_memory_view(count, src)));
    return jarray;
}

/**
 *  Gives read access to a JSArray
 */
template <typename T> class JSSpan {
public:
    JSSpan(JSArray src) {
        const size_t len = src["length"].as<size_t>();
        T* data;

        // If the buffer was allocated via CanvasKit' Malloc, we can peek directly at it!
        if (src["_ck"].isTrue()) {
            fOwned = false;
            data = reinterpret_cast<T*>(src["byteOffset"].as<size_t>());
        } else {
            fOwned = true;
            data = new T[len];

            // now actually copy into 'data'
            if (src.instanceof(emscripten::val::global(JSArrayType<T>::gName))) {
                auto dst_view = emscripten::val(typed_memory_view(len, data));
                dst_view.call<void>("set", src);
            } else {
                for (size_t i = 0; i < len; ++i) {
                    data[i] = src[i].as<T>();
                }
            }
        }
        fSpan = SkSpan(data, len);
    }

    ~JSSpan() {
        if (fOwned) {
            delete[] fSpan.data();
        }
    }

    const T* data() const { return fSpan.data(); }
    size_t size() const { return fSpan.size(); }

private:
    SkSpan<T>   fSpan;
    bool        fOwned;
};

#endif
