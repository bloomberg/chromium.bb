// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "RecordTest.h"

namespace blink {

RecordTest::RecordTest() {}

RecordTest::~RecordTest() {}

void RecordTest::setStringLongRecord(
    const Vector<std::pair<String, int32_t>>& arg) {
  m_stringLongRecord = arg;
}

Vector<std::pair<String, int32_t>> RecordTest::getStringLongRecord() {
  return m_stringLongRecord;
}

void RecordTest::setNullableStringLongRecord(
    const Nullable<Vector<std::pair<String, int32_t>>>& arg) {
  m_nullableStringLongRecord = arg;
}

Nullable<Vector<std::pair<String, int32_t>>>
RecordTest::getNullableStringLongRecord() {
  return m_nullableStringLongRecord;
}

Vector<std::pair<String, String>> RecordTest::getByteStringByteStringRecord() {
  return m_byteStringByteStringRecord;
}

void RecordTest::setByteStringByteStringRecord(
    const Vector<std::pair<String, String>>& arg) {
  m_byteStringByteStringRecord = arg;
}

void RecordTest::setStringElementRecord(
    const HeapVector<std::pair<String, Member<Element>>>& arg) {
  m_stringElementRecord = arg;
}

HeapVector<std::pair<String, Member<Element>>>
RecordTest::getStringElementRecord() {
  return m_stringElementRecord;
}

void RecordTest::setUSVStringUSVStringBooleanRecordRecord(
    const RecordTest::NestedRecordType& arg) {
  m_USVStringUSVStringBooleanRecordRecord = arg;
}

RecordTest::NestedRecordType
RecordTest::getUSVStringUSVStringBooleanRecordRecord() {
  return m_USVStringUSVStringBooleanRecordRecord;
}

Vector<std::pair<String, Vector<String>>>
RecordTest::returnStringByteStringSequenceRecord() {
  Vector<std::pair<String, Vector<String>>> record;
  Vector<String> innerVector1;
  innerVector1.push_back("hello, world");
  innerVector1.push_back("hi, mom");
  record.push_back(std::make_pair(String("foo"), innerVector1));
  Vector<String> innerVector2;
  innerVector2.push_back("goodbye, mom");
  record.push_back(std::make_pair(String("bar"), innerVector2));
  return record;
}

bool RecordTest::unionReceivedARecord(
    const BooleanOrByteStringByteStringRecord& arg) {
  return arg.isByteStringByteStringRecord();
}

DEFINE_TRACE(RecordTest) {
  visitor->trace(m_stringElementRecord);
}

}  // namespace blink
