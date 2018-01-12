// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <type_traits>
#include <utility>
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/HeapTraits.h"
#include "platform/heap/Member.h"
#include "platform/wtf/Vector.h"

// No gtest tests; only static_assert checks.

namespace blink {

class Visitor;

namespace {

struct Empty {};

// Similar to an IDL union or dictionary, which have Trace() methods but are
// not garbage-collected types themselves.
struct StructWithTraceMethod {
  void Trace(blink::Visitor*) {}
};

struct GarbageCollectedStruct
    : public GarbageCollected<GarbageCollectedStruct> {
  void Trace(blink::Visitor*) {}
};

// AddMemberIfNeeded<T>
static_assert(std::is_same<AddMemberIfNeeded<double>::type, double>::value,
              "AddMemberIfNeeded<double> must not add a Member wrapper");
static_assert(std::is_same<AddMemberIfNeeded<double*>::type, double*>::value,
              "AddMemberIfNeeded<double*> must not add a Member wrapper");

static_assert(std::is_same<AddMemberIfNeeded<Empty>::type, Empty>::value,
              "AddMemberIfNeeded<Empty> must not add a Member wrapper");

static_assert(
    std::is_same<AddMemberIfNeeded<StructWithTraceMethod>::type,
                 StructWithTraceMethod>::value,
    "AddMemberIfNeeded<StructWithTraceMethod> must not add a Member wrapper");

static_assert(
    std::is_same<AddMemberIfNeeded<GarbageCollectedStruct>::type,
                 Member<GarbageCollectedStruct>>::value,
    "AddMemberIfNeeded<GarbageCollectedStruct> must not add a Member wrapper");

static_assert(
    std::is_same<
        AddMemberIfNeeded<HeapVector<Member<GarbageCollectedStruct>>>::type,
        Member<HeapVector<Member<GarbageCollectedStruct>>>>::value,
    "AddMemberIfNeeded on a HeapVector<Member<T>> must wrap it in a Member<>");

// VectorOf<T>
static_assert(std::is_same<VectorOf<double>::type, Vector<double>>::value,
              "VectorOf<double> should use a Vector");
static_assert(std::is_same<VectorOf<double*>::type, Vector<double*>>::value,
              "VectorOf<double*> should use a Vector");
static_assert(std::is_same<VectorOf<Empty>::type, Vector<Empty>>::value,
              "VectorOf<Empty> should use a Vector");

static_assert(
    std::is_same<VectorOf<StructWithTraceMethod>::type,
                 HeapVector<StructWithTraceMethod>>::value,
    "VectorOf<StructWithTraceMethod> must not add a Member<> wrapper");
static_assert(std::is_same<VectorOf<GarbageCollectedStruct>::type,
                           HeapVector<Member<GarbageCollectedStruct>>>::value,
              "VectorOf<GarbageCollectedStruct> must add a Member<> wrapper");

static_assert(
    std::is_same<VectorOf<Vector<double>>::type, Vector<Vector<double>>>::value,
    "Nested Vectors must not add HeapVectors");
static_assert(
    std::is_same<VectorOf<HeapVector<StructWithTraceMethod>>::type,
                 HeapVector<Member<HeapVector<StructWithTraceMethod>>>>::value,
    "Nested HeapVector<StructWithTraceMethod> must add a HeapVector");
static_assert(
    std::is_same<
        VectorOf<HeapVector<Member<GarbageCollectedStruct>>>::type,
        HeapVector<Member<HeapVector<Member<GarbageCollectedStruct>>>>>::value,
    "Nested HeapVectors must not add Vectors");

// VectorOfPairs<T, U>
static_assert(std::is_same<VectorOfPairs<int, double>::type,
                           Vector<std::pair<int, double>>>::value,
              "POD types must use a regular Vector");
static_assert(std::is_same<VectorOfPairs<Empty, double>::type,
                           Vector<std::pair<Empty, double>>>::value,
              "POD types must use a regular Vector");

static_assert(
    std::is_same<VectorOfPairs<StructWithTraceMethod, float>::type,
                 HeapVector<std::pair<StructWithTraceMethod, float>>>::value,
    "StructWithTraceMethod causes a HeapVector to be used");
static_assert(
    std::is_same<VectorOfPairs<float, StructWithTraceMethod>::type,
                 HeapVector<std::pair<float, StructWithTraceMethod>>>::value,
    "StructWithTraceMethod causes a HeapVector to be used");
static_assert(
    std::is_same<
        VectorOfPairs<StructWithTraceMethod, StructWithTraceMethod>::type,
        HeapVector<std::pair<StructWithTraceMethod, StructWithTraceMethod>>>::
        value,
    "StructWithTraceMethod causes a HeapVector to be used");

static_assert(
    std::is_same<
        VectorOfPairs<GarbageCollectedStruct, float>::type,
        HeapVector<std::pair<Member<GarbageCollectedStruct>, float>>>::value,
    "GarbageCollectedStruct causes a HeapVector to be used");
static_assert(
    std::is_same<
        VectorOfPairs<float, GarbageCollectedStruct>::type,
        HeapVector<std::pair<float, Member<GarbageCollectedStruct>>>>::value,
    "GarbageCollectedStruct causes a HeapVector to be used");
static_assert(
    std::is_same<
        VectorOfPairs<GarbageCollectedStruct, GarbageCollectedStruct>::type,
        HeapVector<std::pair<Member<GarbageCollectedStruct>,
                             Member<GarbageCollectedStruct>>>>::value,
    "GarbageCollectedStruct causes a HeapVector to be used");

}  // namespace

}  // namespace blink
