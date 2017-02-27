# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import

"""Unit tests for idl_types.py."""

import unittest

from idl_types import IdlRecordType
from idl_types import IdlSequenceType
from idl_types import IdlType


class IdlTypeTest(unittest.TestCase):

    def test_is_void(self):
        idl_type = IdlType('void')
        self.assertTrue(idl_type.is_void)
        idl_type = IdlType('somethingElse')
        self.assertFalse(idl_type.is_void)


class IdlRecordTypeTest(unittest.TestCase):

    def test_idl_types(self):
        idl_type = IdlRecordType(IdlType('USVString'), IdlType('long'))
        idl_types = list(idl_type.idl_types())
        self.assertEqual(len(idl_types), 3)
        self.assertIs(idl_types[0], idl_type)
        self.assertEqual(idl_types[1].name, 'USVString')
        self.assertEqual(idl_types[2].name, 'Long')
        self.assertListEqual(list(idl_type.idl_types()),
                             [idl_type, idl_type.key_type, idl_type.value_type])

        idl_type = IdlRecordType(IdlType('DOMString'), IdlSequenceType(IdlType('unrestricted float')))
        idl_types = list(idl_type.idl_types())
        self.assertEqual(len(idl_types), 4)
        self.assertIs(idl_types[0], idl_type)
        self.assertEqual(idl_types[1].name, 'String')
        self.assertEqual(idl_types[2].name, 'UnrestrictedFloatSequence')
        self.assertEqual(idl_types[3].name, 'UnrestrictedFloat')
        self.assertListEqual(list(idl_type.idl_types()),
                             [idl_type, idl_type.key_type, idl_type.value_type, idl_type.value_type.element_type])

        idl_type = IdlRecordType(IdlType('ByteString'),
                                 IdlRecordType(IdlType('DOMString'), IdlType('octet')))
        idl_types = list(idl_type.idl_types())
        self.assertEqual(len(idl_types), 5)
        self.assertIs(idl_types[0], idl_type)
        self.assertEqual(idl_types[1].name, 'ByteString')
        self.assertEqual(idl_types[2].name, 'StringOctetRecord')
        self.assertEqual(idl_types[3].name, 'String')
        self.assertEqual(idl_types[4].name, 'Octet')
        self.assertListEqual(list(idl_type.idl_types()),
                             [idl_type, idl_type.key_type, idl_type.value_type, idl_type.value_type.key_type,
                              idl_type.value_type.value_type])

    def test_is_record(self):
        idl_type = IdlType('USVString')
        self.assertFalse(idl_type.is_record_type)
        idl_type = IdlSequenceType(IdlRecordType(IdlType('DOMString'), IdlType('byte')))
        self.assertFalse(idl_type.is_record_type)
        idl_type = IdlRecordType(IdlType('USVString'), IdlType('long'))
        self.assertTrue(idl_type.is_record_type)
        idl_type = IdlRecordType(IdlType('USVString'), IdlSequenceType(IdlType('boolean')))
        self.assertTrue(idl_type.is_record_type)

    def test_name(self):
        idl_type = IdlRecordType(IdlType('ByteString'), IdlType('octet'))
        self.assertEqual(idl_type.name, 'ByteStringOctetRecord')
        idl_type = IdlRecordType(IdlType('USVString'), IdlSequenceType(IdlType('double')))
        self.assertEqual(idl_type.name, 'USVStringDoubleSequenceRecord')
        idl_type = IdlRecordType(IdlType('DOMString'),
                                 IdlRecordType(IdlType('ByteString'),
                                               IdlSequenceType(IdlType('unsigned short'))))
        self.assertEqual(idl_type.name, 'StringByteStringUnsignedShortSequenceRecordRecord')
