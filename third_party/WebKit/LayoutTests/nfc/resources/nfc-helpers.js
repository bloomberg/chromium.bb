'use strict';

var test_text_data = 'Test text data.';
var test_text_byte_array = new TextEncoder('utf-8').encode(test_text_data);
var test_number_data = 42;
var test_json_data = {level: 1, score: 100, label: 'Game'};
var test_url_data = 'https://w3c.github.io/web-nfc/';
var test_message_origin = 'https://127.0.0.1:8443';
var test_buffer_data = new ArrayBuffer(test_text_byte_array.length);
var test_buffer_view = new Uint8Array(test_buffer_data).set(test_text_byte_array);

var NFCHWStatus = {};
NFCHWStatus.ENABLED = 1;
NFCHWStatus.NOT_SUPPORTED = NFCHWStatus.ENABLED + 1;
NFCHWStatus.DISABLED = NFCHWStatus.NOT_SUPPORTED + 1;

function noop() {}

function assertRejectsWithError(promise, name) {
  return promise.then(() => {
    assert_unreached('expected promise to reject with ' + name);
  }, error => {
    assert_equals(error.name, name);
  });
}

function createMessage(records) {
  if (records !== undefined) {
    let message = {}
    message.records = records;
    return message;
  }
}

function createRecord(recordType, mediaType, data) {
  let record = {};
  if (recordType !== undefined)
    record.recordType = recordType;
  if (mediaType !== undefined)
    record.mediaType = mediaType;
  if (data !== undefined)
    record.data = data;
  return record;
}

function createNFCPushOptions(target, timeout, ignoreRead) {
  return { target, timeout, ignoreRead };
}

function createNFCWatchOptions(url, recordType, mediaType, mode) {
  return { url, recordType, mediaType, mode};
}

function createTextRecord(text) {
  return createRecord('text', 'text/plain', text);
}

function createJsonRecord(json) {
  return createRecord('json', 'application/json', json);
}

function createOpaqueRecord(buffer) {
  return createRecord('opaque', 'application/octet-stream', buffer);
}

function createUrlRecord(url) {
  return createRecord('url', 'text/plain', url);
}

function nfc_mocks(mojo) {
  return define('NFC mocks', [
    'mojo/public/js/bindings',
    'services/device/public/interfaces/nfc.mojom',
  ], (bindings, nfc) => {

    function toMojoNFCRecordType(type) {
      switch (type) {
      case 'text':
        return nfc.NFCRecordType.TEXT;
      case 'url':
        return nfc.NFCRecordType.URL;
      case 'json':
        return nfc.NFCRecordType.JSON;
      case 'opaque':
        return nfc.NFCRecordType.OPAQUE_RECORD;
      }

      return nfc.NFCRecordType.EMPTY;
    }

    function toMojoNFCPushTarget(target) {
      switch (target) {
      case 'any':
        return nfc.NFCPushTarget.ANY;
      case 'peer':
        return nfc.NFCPushTarget.PEER;
      case 'tag':
        return nfc.NFCPushTarget.TAG;
      }

      return nfc.NFCPushTarget.ANY;
    }

    function toMojoNFCWatchMode(mode) {
      if (mode === 'web-nfc-only')
        return nfc.NFCWatchMode.WEBNFC_ONLY;
      return nfc.NFCWatchMode.ANY;
    }

    // Converts between NFCMessage https://w3c.github.io/web-nfc/#dom-nfcmessage
    // and mojo::NFCMessage structure, so that nfc.watch function can be tested.
    function toMojoNFCMessage(message) {
      let nfcMessage = new nfc.NFCMessage();
      nfcMessage.url = message.url;
      nfcMessage.data = [];
      for (let record of message.records)
        nfcMessage.data.push(toMojoNFCRecord(record));
      return nfcMessage;
    }

    function toMojoNFCRecord(record) {
      let nfcRecord = new nfc.NFCRecord();
      nfcRecord.record_type = toMojoNFCRecordType(record.recordType);
      nfcRecord.media_type = record.mediaType;
      nfcRecord.data = toByteArray(record.data);
      return nfcRecord;
    }

    function toByteArray(data) {
      // Convert JS objects to byte array
      let byteArray;
      let tmpData = data;

      if (tmpData instanceof ArrayBuffer)
        byteArray = new Uint8Array(tmpData);
      else if (typeof tmpData === 'object' || typeof tmpData === 'number')
        tmpData = JSON.stringify(tmpData);

      if (typeof tmpData === 'string')
        byteArray = new TextEncoder('utf-8').encode(tmpData);

      return byteArray;
    }

    // Compares NFCMessage that was provided to the API
    // (e.g. navigator.nfc.push), and NFCMessage that was received by the
    // mock NFC service.
    function assertNFCMessagesEqual(providedMessage, receivedMessage) {
      // If simple data type is passed, e.g. String or ArrayBuffer, convert it
      // to NFCMessage before comparing.
      // https://w3c.github.io/web-nfc/#idl-def-nfcpushmessage
      let provided = providedMessage;
      if (providedMessage instanceof ArrayBuffer)
        provided = createMessage([createOpaqueRecord(providedMessage)]);
      else if (typeof providedMessage === 'string')
        provided = createMessage([createTextRecord(providedMessage)]);

      assert_equals(provided.records.length, receivedMessage.data.length,
          'NFCMessages must have same number of NFCRecords');

      // Compare contents of each individual NFCRecord
      for (let i = 0; i < provided.records.length; ++i)
        compareNFCRecords(provided.records[i], receivedMessage.data[i]);
    }

    // Used to compare two WebNFC messages, one that is provided to mock NFC
    // service through triggerWatchCallback and another that is received by
    // callback that is provided to navigator.nfc.watch function.
    function assertWebNFCMessagesEqual(a, b) {
      assert_equals(a.url, b.url);
      assert_equals(a.records.length, b.records.length);
      for(let i in a.records) {
        let recordA = a.records[i];
        let recordB = b.records[i];
        assert_equals(recordA.recordType, recordB.recordType);
        assert_equals(recordA.mediaType, recordB.mediaType);

        if (recordA.data instanceof ArrayBuffer) {
          assert_array_equals(new Uint8Array(recordA.data),
              new Uint8Array(recordB.data));
        } else if (typeof recordA.data === 'object') {
          assert_object_equals(recordA.data, recordB.data);
        }

        if (typeof recordA.data === 'number'
            || typeof recordA.data === 'string') {
          assert_true(recordA.data == recordB.data);
        }
      }
    }

    // Compares NFCRecords that were provided / received by the mock service.
    function compareNFCRecords(providedRecord, receivedRecord) {
      assert_equals(toMojoNFCRecordType(providedRecord.recordType),
                    receivedRecord.record_type);

      // Compare media types without charset.
      // Charset should be compared when watch method is implemented, in order
      // to check that written and read strings are equal.
      assert_equals(providedRecord.mediaType,
          receivedRecord.media_type.substring(0, providedRecord.mediaType.length));

      assert_false(toMojoNFCRecordType(providedRecord.recordType) ==
                  nfc.NFCRecordType.EMPTY);

      assert_array_equals(toByteArray(providedRecord.data),
                          new Uint8Array(receivedRecord.data));
    }

    // Compares NFCPushOptions structures that were provided to API and
    // received by the mock mojo service.
    function assertNFCPushOptionsEqual(provided, received) {
      if (provided.ignoreRead !== undefined)
        assert_equals(provided.ignoreRead, !!+received.ignore_read);
      else
        assert_equals(!!+received.ignore_read, true);

      if (provided.timeout !== undefined)
        assert_equals(provided.timeout, received.timeout);
      else
        assert_equals(received.timeout, Infinity);

      if (provided.target !== undefined)
        assert_equals(toMojoNFCPushTarget(provided.target), received.target);
      else
        assert_equals(received.target, nfc.NFCPushTarget.ANY);
    }

    // Compares NFCWatchOptions structures that were provided to API and
    // received by the mock mojo service.
    function assertNFCWatchOptionsEqual(provided, received) {
      if (provided.url !== undefined)
        assert_equals(provided.url, received.url);
      else
        assert_equals(received.url, '');

      if (provided.mediaType !== undefined)
        assert_equals(provided.mediaType, received.media_type);
      else
        assert_equals(received.media_type, '');

      if (provided.mode !== undefined)
        assert_equals(toMojoNFCWatchMode(provided.mode), received.mode);
      else
        assert_equals(received.mode, nfc.NFCWatchMode.WEBNFC_ONLY);

      if (provided.recordType !== undefined) {
        assert_equals(!+received.record_filter, true);
        assert_equals(toMojoNFCRecordType(provided.recordType),
            received.record_filter.record_type);
      }
    }

    function createNFCError(type) {
      return { error: type ?
          new nfc.NFCError({ error_type: type }) : null };
    }

    class MockNFC {
      constructor() {
        this.bindingSet = new bindings.BindingSet(nfc.NFC);

        this.hw_status_ = NFCHWStatus.ENABLED;
        this.pushed_message_ = null;
        this.push_options_ = null;
        this.pending_promise_func_ = null;
        this.push_timeout_id_ = null;
        this.push_completed_ = true;
        this.client_ = null;
        this.watch_id_ = 0;
        this.watchers_ = [];
      }

      // NFC delegate functions
      push(message, options) {
        let error = this.isReady();
        if (error)
          return Promise.resolve(error);

        this.pushed_message_ = message;
        this.push_options_ = options;

        return new Promise((resolve, reject) => {
          this.pending_promise_func_ = resolve;
          if (options.timeout && options.timeout !== Infinity &&
              !this.push_completed_) {
            this.push_timeout_id_ =
                window.setTimeout(() => {
                    resolve(createNFCError(nfc.NFCErrorType.TIMER_EXPIRED));
                }, options.timeout);
          } else {
            resolve(createNFCError(null));
          }
        });
      }

      cancelPush(target) {
        if (this.push_options_ && ((target === nfc.NFCPushTarget.ANY) ||
            (this.push_options_.target === target))) {
          this.cancelPendingPushOperation();
        }

        return Promise.resolve(createNFCError(null));
      }

      setClient(client) {
        this.client_ = client;
      }

      watch(options) {
        let error = this.isReady();
        if (error) {
          error.id = 0;
          return Promise.resolve(error);
        }

        let retVal = createNFCError(null);
        retVal.id = ++this.watch_id_;
        this.watchers_.push({id: this.watch_id_, options: options});
        return Promise.resolve(retVal);
      }

      cancelWatch(id) {
        let index = this.watchers_.findIndex(value => value.id === id);
        if (index === -1) {
          return Promise.resolve(createNFCError(nfc.NFCErrorType.NOT_FOUND));
        }

        this.watchers_.splice(index, 1);
        return Promise.resolve(createNFCError(null));
      }

      cancelAllWatches() {
        if (this.watchers_.length === 0) {
          return Promise.resolve(createNFCError(nfc.NFCErrorType.NOT_FOUND));
        }

        this.watchers_.splice(0, this.watchers_.length);
        return Promise.resolve(createNFCError(null));
      }

      isReady() {
        if (this.hw_status_ === NFCHWStatus.DISABLED)
          return createNFCError(nfc.NFCErrorType.DEVICE_DISABLED);
        if (this.hw_status_ === NFCHWStatus.NOT_SUPPORTED)
          return createNFCError(nfc.NFCErrorType.NOT_SUPPORTED);
        return null;
      }

      setHWStatus(status) {
        this.hw_status_ = status;
      }

      pushedMessage() {
        return this.pushed_message_;
      }

      pushOptions() {
        return this.push_options_;
      }

      watchOptions() {
        assert_not_equals(this.watchers_.length, 0);
        return this.watchers_[this.watchers_.length - 1].options;
      }

      setPendingPushCompleted(result) {
        this.push_completed_ = result;
      }

      reset() {
        this.hw_status_ = NFCHWStatus.ENABLED;
        this.push_completed_ = true;
        this.watch_id_ = 0;
        this.watchers_ = [];
        this.cancelPendingPushOperation();
      }

      cancelPendingPushOperation() {
        if (this.push_timeout_id_) {
          window.clearTimeout(this.push_timeout_id_);
        }

        if (this.pending_promise_func_) {
          this.pending_promise_func_(createNFCError(nfc.NFCErrorType.OPERATION_CANCELLED));
        }

        this.pushed_message_ = null;
        this.push_options_ = null;
        this.pending_promise_func_ = null;
      }

      triggerWatchCallback(id, message) {
        assert_true(this.client_ !== null);
        if (this.watchers_.length > 0) {
          this.client_.onWatch([id], toMojoNFCMessage(message));
        }
      }
    }

    let mockNFC = new MockNFC;
    mojo.frameInterfaces.addInterfaceOverrideForTesting(
        nfc.NFC.name,
        handle => {
          mockNFC.bindingSet.addBinding(mockNFC, handle);
        });

    return Promise.resolve({
      // Interface instance bound to main frame.
      mockNFC: mockNFC,
      // Constructor for mock NFC class.
      MockNFC: MockNFC,
      // Loaded mojom interface.
      NFC: nfc.NFC,
      // Helper functions for comparing WebNFC structures.
      assertNFCMessagesEqual: assertNFCMessagesEqual,
      assertNFCPushOptionsEqual: assertNFCPushOptionsEqual,
      assertWebNFCMessagesEqual: assertWebNFCMessagesEqual,
      assertNFCWatchOptionsEqual: assertNFCWatchOptionsEqual,
    });
  });
}

function nfc_test(func, name, properties) {
  mojo_test(mojo => nfc_mocks(mojo).then(nfc => {
    let result = Promise.resolve(func(nfc));
    let cleanUp = () => nfc.mockNFC.reset();
    result.then(cleanUp, cleanUp);
    return result;
  }), name, properties);
}
