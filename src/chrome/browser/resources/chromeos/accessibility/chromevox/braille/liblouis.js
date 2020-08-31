// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview JavaScript shim for the liblouis Native Client wrapper.
 */

goog.provide('LibLouis');
goog.provide('LibLouis.FormType');

/**
 * Encapsulates a liblouis Native Client instance in the page.
 */
LibLouis = class {
  /**
   * @param {string} wasmPath Path to .wasm file for the module.
   * @param {string=} opt_tablesDir Path to tables directory.
   * @param {function(LibLouis)=} opt_loadCallback
   */
  constructor(wasmPath, opt_tablesDir, opt_loadCallback) {
    /**
     * Path to .wasm file for the module.
     * @private {string}
     */
    this.wasmPath_ = wasmPath;

    /** @private {Worker} */
    this.worker_;

    /**
     * Path to translation tables.
     * @private {?string}
     */
    this.tablesDir_ = goog.isDef(opt_tablesDir) ? opt_tablesDir : null;


    /**
     * Whether liblouis is loaded.
     * @private {boolean}
     */
    this.isLoaded_ = false;

    /**
     * Pending RPC callbacks. Maps from message IDs to callbacks.
     * @private {!Object<function(!Object)>}
     */
    this.pendingRpcCallbacks_ = {};

    /**
     * Next message ID to be used. Incremented with each sent message.
     * @private {number}
     */
    this.nextMessageId_ = 1;

    this.loadOrReload_(opt_loadCallback);
  }

  isLoaded() {
    return this.isLoaded_;
  }

  /**
   * Returns a translator for the desired table, asynchronously.
   * This object must be attached to a document when requesting a translator.
   * @param {string} tableNames Comma separated list of braille table names for
   *     liblouis.
   * @param {function(LibLouis.Translator)} callback
   *     Callback which will receive the translator, or {@code null} on failure.
   */
  getTranslator(tableNames, callback) {
    if (!this.isLoaded_) {
      // TODO: save last callback.
      return;
    }
    this.rpc_('CheckTable', {'table_names': tableNames}, function(reply) {
      if (reply['success']) {
        const translator = new LibLouis.Translator(this, tableNames);
        callback(translator);
      } else {
        callback(null /* translator */);
      }
    }.bind(this));
  }

  /**
   * Dispatches a message to the remote end and returns the reply
   * asynchronously. A message ID will be automatically assigned (as a
   * side-effect).
   * @param {string} command Command name to be sent.
   * @param {!Object} message JSONable message to be sent.
   * @param {function(!Object)} callback Callback to receive the reply.
   * @private
   */
  rpc_(command, message, callback) {
    if (!this.worker_) {
      throw Error('Cannot send RPC: liblouis instance not loaded');
    }
    const messageId = '' + this.nextMessageId_++;
    message['message_id'] = messageId;
    message['command'] = command;
    const json = JSON.stringify(message);
    if (LibLouis.DEBUG) {
      window.console.debug('RPC -> ' + json);
    }
    this.worker_.postMessage(json);
    this.pendingRpcCallbacks_[messageId] = callback;
  }

  /**
   * Invoked when the Native Client instance successfully loads.
   * @param {Event} e Event dispatched after loading.
   * @private
   */
  onInstanceLoad_(e) {
    window.console.info('loaded liblouis Native Client instance');
  }

  /**
   * Invoked when the Native Client instance fails to load.
   * @param {Event} e Event dispatched after loading failure.
   * @private
   */
  onInstanceError_(e) {
    window.console.error('Error in liblouis ' + e.message);
    this.loadOrReload_();
  }

  /**
   * Invoked when the Native Client instance posts a message.
   * @param {Event} e Event dispatched after the message was posted.
   * @private
   */
  onInstanceMessage_(e) {
    if (LibLouis.DEBUG) {
      window.console.debug('RPC <- ' + e.data);
    }
    const message = /** @type {!Object} */ (JSON.parse(e.data));
    const messageId = message['in_reply_to'];
    if (!goog.isDef(messageId)) {
      window.console.warn(
          'liblouis Native Client module sent message with no ID', message);
      return;
    }
    if (goog.isDef(message['error'])) {
      window.console.error('liblouis Native Client error', message['error']);
    }
    const callback = this.pendingRpcCallbacks_[messageId];
    if (goog.isDef(callback)) {
      delete this.pendingRpcCallbacks_[messageId];
      callback(message);
    }
  }

  /**
   * @param {function(LibLouis)=} opt_loadCallback
   * @private
   */
  loadOrReload_(opt_loadCallback) {
    this.worker_ = new Worker(this.wasmPath_);
    this.worker_.addEventListener(
        'message', goog.bind(this.onInstanceMessage_, this),
        false /* useCapture */);
    this.worker_.addEventListener(
        'error', goog.bind(this.onInstanceError_, this),
        false /* useCapture */);
    this.rpc_('load', {}, () => {
      this.isLoaded_ = true;
      opt_loadCallback && opt_loadCallback(this);
    });
  }
};


/**
 * Constants taken from liblouis.h.
 * Controls braille indicator insertion during translation.
 * @enum {number}
 */
LibLouis.FormType = {
  PLAIN_TEXT: 0,
  ITALIC: 1,
  UNDERLINE: 2,
  BOLD: 4,
  COMPUTER_BRAILLE: 8
};


/**
 * Set to {@code true} to enable debug logging of RPC messages.
 * @type {boolean}
 */
LibLouis.DEBUG = false;


/**
 * Braille translator which uses a Native Client instance of liblouis.
 */
LibLouis.Translator = class {
  /**
   * @param {!LibLouis} instance The instance wrapper.
   * @param {string} tableNames Comma separated list of Table names to be passed
   *     to liblouis.
   */
  constructor(instance, tableNames) {
    /**
     * The instance wrapper.
     * @private {!LibLouis}
     */
    this.instance_ = instance;

    /**
     * The table name.
     * @private {string}
     */
    this.tableNames_ = tableNames;
  }

  /**
   * Translates text into braille cells.
   * @param {string} text Text to be translated.
   * @param {function(ArrayBuffer, Array<number>, Array<number>)} callback
   *     Callback for result.  Takes 3 parameters: the resulting cells,
   *     mapping from text to braille positions and mapping from braille to
   *     text positions.  If translation fails for any reason, all parameters
   * are
   *     {@code null}.
   */
  translate(text, formTypeMap, callback) {
    if (!this.instance_.worker_) {
      callback(null /*cells*/, null /*textToBraille*/, null /*brailleToText*/);
      return;
    }
    const message = {
      'table_names': this.tableNames_,
      text,
      form_type_map: formTypeMap
    };
    this.instance_.rpc_('Translate', message, function(reply) {
      let cells = null;
      let textToBraille = null;
      let brailleToText = null;
      if (reply['success'] && goog.isString(reply['cells'])) {
        cells = LibLouis.Translator.decodeHexString_(reply['cells']);
        if (goog.isDef(reply['text_to_braille'])) {
          textToBraille = reply['text_to_braille'];
        }
        if (goog.isDef(reply['braille_to_text'])) {
          brailleToText = reply['braille_to_text'];
        }
      } else if (text.length > 0) {
        // TODO(plundblad): The nacl wrapper currently returns an error
        // when translating an empty string.  Address that and always log here.
        console.error(
            'Braille translation error for ' + JSON.stringify(message));
      }
      callback(cells, textToBraille, brailleToText);
    });
  }

  /**
   * Translates braille cells into text.
   * @param {!ArrayBuffer} cells Cells to be translated.
   * @param {function(?string)} callback Callback for result.
   */
  backTranslate(cells, callback) {
    if (!this.instance_.worker_) {
      callback(null /*text*/);
      return;
    }
    if (cells.byteLength == 0) {
      // liblouis doesn't handle empty input, so handle that trivially
      // here.
      callback('');
      return;
    }
    const message = {
      'table_names': this.tableNames_,
      'cells': LibLouis.Translator.encodeHexString_(cells)
    };
    this.instance_.rpc_('BackTranslate', message, function(reply) {
      if (reply['success'] && goog.isString(reply['text'])) {
        callback(reply['text']);
      } else {
        callback(null /* text */);
      }
    });
  }

  /**
   * Decodes a hexadecimal string to an {@code ArrayBuffer}.
   * @param {string} hex Hexadecimal string.
   * @return {!ArrayBuffer} Decoded binary data.
   * @private
   */
  static decodeHexString_(hex) {
    if (!/^([0-9a-f]{2})*$/i.test(hex)) {
      throw Error('invalid hexadecimal string');
    }
    const array = new Uint8Array(hex.length / 2);
    let idx = 0;
    for (let i = 0; i < hex.length; i += 2) {
      array[idx++] = parseInt(hex.substring(i, i + 2), 16);
    }
    return array.buffer;
  }

  /**
   * Encodes an {@code ArrayBuffer} in hexadecimal.
   * @param {!ArrayBuffer} arrayBuffer Binary data.
   * @return {string} Hexadecimal string.
   * @private
   */
  static encodeHexString_(arrayBuffer) {
    const array = new Uint8Array(arrayBuffer);
    let hex = '';
    for (let i = 0; i < array.length; i++) {
      const b = array[i];
      hex += (b < 0x10 ? '0' : '') + b.toString(16);
    }
    return hex;
  }
};
