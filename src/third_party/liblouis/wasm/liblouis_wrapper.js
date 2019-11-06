// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Liblouis wrapper.
 */

/**
 * A utility class that acts as a memory pool and wraps the wasm module.
 */
function WasmMemPool(module) {
  this.module = module;

  this.ptrs_ = [];
}

WasmMemPool.prototype = {
  malloc: function(bytes) {
    let ptr = this.module._malloc(bytes);
    this.ptrs_.push(ptr);
    return ptr;
  },

  allocate: function(array, type) {
    let ptr = this.module.allocate(array, type, this.module.ALLOC_NORMAL);
    this.ptrs_.push(ptr);
    return ptr;
  },

  freeAll: function() {
    this.ptrs_.forEach((ptr) => {
      this.module._free(ptr);
    });
    this.ptrs_ = [];
  }
};

function LiblouisWrapper() {
  self.addEventListener('message', (message) => {
    var command = JSON.parse(message.data);
    switch (command.command) {
      case 'CheckTable':
        this.checkTable(command);
        break;
      case 'Translate':
        this.translate(command);
        break;
      case 'BackTranslate':
        this.backTranslate(command);
        break;
      case 'load':
        this.initWasm(command);
        break;
    }
  });

  importScripts('liblouis_wasm.js');
}

LiblouisWrapper.prototype = {
  initWasm: function(message) {
    if (!self.liblouisBuild) {
      setTimeout(this.initWasm.bind(this, message), 20);
      return;
    }
    liblouisBuild().then((module) => {
      this.module = module;
      this.pool_ = new WasmMemPool(this.module);

      let reply = {
        loaded: true,
        success: true,
        in_reply_to: message['message_id']
      };
      self.postMessage(JSON.stringify(reply));
    });
  },


  checkTable: function(command) {
    // Free any loaded tables.
    this.module._lou_free();

    let tableNames = command['table_names'];
    let tableNamesPtr =
        this.pool_.allocate(this.module.intArrayFromString(tableNames), 'i8');
    let tableCount = this.module._lou_checkTable(tableNamesPtr);
    this.pool_.freeAll();
    let msg = {in_reply_to: command['message_id'],
               success: tableCount > 0};
    self.postMessage(JSON.stringify(msg));
  },

  translate: function(command) {
    this.translateOrBackTranslate_(
        command['table_names'], command['text'], command['message_id'],
        command['form_type_map']);
  },

  backTranslate: function(command) {
    this.translateOrBackTranslate_(
        command['table_names'], command['cells'], command['message_id'], null,
        true);
  },

  translateOrBackTranslate_: function(
      tableNames, contents, messageId, formTypeMap, backTranslate) {
    let tableNamesPtr =
        this.pool_.allocate(this.module.intArrayFromString(tableNames), 'i8');

    let formTypeMapPtr = 0;
    if (formTypeMap) {
      formTypeMapPtr = this.pool_.malloc(formTypeMap.length * 4);
      for (let i = 0; i < formTypeMap.length; i++)
        this.module.setValue(formTypeMapPtr + i * 4, formTypeMap[i], 'i32');
    }

    // |tableNamesPtr| is a char* natively.

    let inLen = backTranslate ? (contents.length / 2 + 1) : contents.length;

    // |inBufPtr| and |outBufPtr| are both widechar*.
    let inBufPtr = this.pool_.malloc(inLen * 2);
    if (backTranslate) {
      // |contents| is a hex encoded string. Two characters ecode a byte.
      if (contents.length % 2 != 0)
        throw 'Expected contents to be of even length.';

      for (let i = 0; i < contents.length; i = i + 2) {
        // Always set the high order bit to ensure empty cells are not ignored.
        let twoBytes = 0x8000;
        twoBytes |= parseInt(contents[i], 16) << 4;
        twoBytes |= parseInt(contents[i + 1], 16);
        this.module.setValue(inBufPtr + i, twoBytes, 'i16');
      }

      // Liblouis expects a null terminator.
      this.module.setValue(inBufPtr + (inLen - 1) * 2, 0, 'i16');
    } else {
      this.module.stringToUTF16(contents, inBufPtr, inLen * 2 + 1);
    }

    let inLenPtr = this.pool_.malloc(4);

    // We need to gradually increase |outLen| since we can't precompute the
    // length given by liblouis.
    let outLen = inLen + 1;
    let maxAlloc = (inLen + 1) * 8;
    while (outLen < maxAlloc) {
      this.module.setValue(inLenPtr, inLen, 'i32');
      let outBufPtr = this.pool_.malloc(outLen * 2);
      let outLenPtr = this.pool_.malloc(4);
      this.module.setValue(outLenPtr, outLen, 'i32');
      let brailleToTextPtr;
      let textToBraillePtr;
      if (backTranslate) {
        this.module._lou_backTranslateString(
            tableNamesPtr, inBufPtr, inLenPtr, outBufPtr, outLenPtr, 0, 0,
            4 /* dots */);
      } else {
        // These two refer to an array of integers.
        brailleToTextPtr = this.pool_.malloc(outLen * 4);
        textToBraillePtr = this.pool_.malloc(outLen * 4);

        this.module._lou_translate(
            tableNamesPtr, inBufPtr, inLenPtr, outBufPtr, outLenPtr,
            formTypeMapPtr, 0, textToBraillePtr, brailleToTextPtr, 0,
            4 /* dots */);
      }

      // If the entire inBuf was not consumed, it means outBuf was not large
      // enough, so we need to try again.
      let actualInLen = this.module.getValue(inLenPtr, 'i32');
      let actualOutLen = this.module.getValue(outLenPtr, 'i32');

      if (inLen == actualInLen && actualOutLen <= outLen) {
        let msg = {in_reply_to: messageId, success: true};
        if (backTranslate) {
          let outBuf = '';
          for (let i = 0; i < actualOutLen; i++) {
            outBuf += String.fromCharCode(
                this.module.getValue(outBufPtr + i * 2, 'i16'));
          }
          msg['text'] = outBuf;
        } else {
          msg['cells'] = this.getHexEncoding_(outBufPtr, actualOutLen);
          msg['text_to_braille'] =
              this.getIntArray(textToBraillePtr, actualInLen);
          msg['braille_to_text'] =
              this.getIntArray(brailleToTextPtr, actualOutLen);
        }
        self.postMessage(JSON.stringify(msg));
        break;
      }

      outLen = outLen * 2;
    }

    this.pool_.freeAll();
  },

  getHexEncoding_: function(bufPtr, len) {
    let ret = '';
    for (let i = 0; i < len; i++) {
      // Note that pointer arithmetic here is in bytes. Each cell is encoded in
      // 16-bits.
      let byte = this.module.getValue(bufPtr + i * 2);

      // Ignore the high order bits.
      byte &= 0x00ff;
      ret += LiblouisWrapper.BYTE_TO_HEX[byte >> 4];
      ret += LiblouisWrapper.BYTE_TO_HEX[byte & 0x0f];
    }
    return ret;
  },

  getIntArray: function(ptr, len) {
    let ret = [];
    for (let i = 0; i < len; i++) {
      ret.push(this.module.getValue(ptr + i * 4, 'i32'));
    }
    return ret;
  }
};

LiblouisWrapper.BYTE_TO_HEX = '0123456789abcdef';

new LiblouisWrapper();
