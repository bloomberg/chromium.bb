// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js

'use strict';

// https://w3c.github.io/web-nfc/

idl_test(
  ['web-nfc'],
  ['html'],
  idl_array => {
    idl_array.add_objects({
      Navigator: ['navigator'],
      NFC: ['navigator.nfc'],
    });
  },
  'Test IDL implementation of Web NFC API');
