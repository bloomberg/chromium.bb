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
    message.data = records;
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
    'mojo/public/js/connection',
    'device/nfc/nfc.mojom',
  ], (bindings, connection, nfc) => {

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

      assert_equals(provided.data.length, receivedMessage.data.length,
          'NFCMessages must have same number of NFCRecords');

      // Compare contents of each individual NFCRecord
      for (let i = 0; i < provided.data.length; ++i)
        compareNFCRecords(provided.data[i], receivedMessage.data[i]);
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

    function createNFCError(type) {
      return { error: type ?
          new nfc.NFCError({ error_type: type }) : null };
    }

    class MockNFC {
      constructor() {
        this.hw_status_ = NFCHWStatus.ENABLED;
        this.pushed_message_ = null;
        this.push_options_ = null;
        this.pending_promise_func_ = null;
        this.push_timeout_id_ = null;
        this.push_completed_ = true;
      }

      // NFC.stubClass delegate functions
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

      // Mock utility functions
      bindToPipe(pipe) {
        this.stub_ = connection.bindHandleToStub(
            pipe, nfc.NFC);
        bindings.StubBindings(this.stub_).delegate = this;
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

      setPendingPushCompleted(result) {
        this.push_completed_ = result;
      }

      reset() {
        this.hw_status_ = NFCHWStatus.ENABLED;
        this.push_completed_ = true;
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
    }

    let mockNFC = new MockNFC;
    mojo.frameInterfaces.addInterfaceOverrideForTesting(
        nfc.NFC.name,
        pipe => {
          mockNFC.bindToPipe(pipe);
        });

    return Promise.resolve({
      mockNFC: mockNFC,
      assertNFCMessagesEqual: assertNFCMessagesEqual,
      assertNFCPushOptionsEqual: assertNFCPushOptionsEqual,
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
