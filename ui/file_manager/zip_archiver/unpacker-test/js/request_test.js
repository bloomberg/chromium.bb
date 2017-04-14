// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

describe('On calling', function() {
  /**
   * @const {string}
   */
  var FILE_SYSTEM_ID = 'id';

  /**
   * @const {number}
   */
  var REQUEST_ID = 10;

  /**
   * @const {string}
   */
  var ENCODING = 'CP1250';

  /**
   * @const {number}
   */
  var ARCHIVE_SIZE = 5000;

  /**
   * @const {!ArrayBuffer}
   */
  var CHUNK_BUFFER = new ArrayBuffer(5);

  /**
   * @const {number}
   */
  var CHUNK_OFFSET = 150;

  /**
   * @const {string}
   */
  var CLOSE_VOLUME_REQUEST_ID = '-1';

  /**
   * @const {number}
   */
  var INDEX = 123;

  /**
   * @const {number}
   */
  var OPEN_REQUEST_ID = 7;

  /**
   * @const {number}
   */
  var OFFSET = 50;

  /**
   * @const {number}
   */
  var LENGTH = 200;

  describe('request.createReadMetadataRequest should create a request',
           function() {
    var readMetadataRequest;
    beforeEach(function() {
      readMetadataRequest = unpacker.request.createReadMetadataRequest(
          FILE_SYSTEM_ID, REQUEST_ID, ENCODING, ARCHIVE_SIZE);
    });

    it('with READ_METADATA as operation', function() {
      expect(readMetadataRequest[unpacker.request.Key.OPERATION])
          .to.equal(unpacker.request.Operation.READ_METADATA);
    });

    it('with correct file system id', function() {
      expect(readMetadataRequest[unpacker.request.Key.FILE_SYSTEM_ID])
          .to.equal(FILE_SYSTEM_ID);
    });

    it('with correct request id', function() {
      expect(readMetadataRequest[unpacker.request.Key.REQUEST_ID])
          .to.equal(REQUEST_ID.toString());
    });

    it('with correct encoding', function() {
      expect(readMetadataRequest[unpacker.request.Key.ENCODING])
          .to.equal(ENCODING);
    });

    it('with correct archive size', function() {
      expect(readMetadataRequest[unpacker.request.Key.ARCHIVE_SIZE])
          .to.equal(ARCHIVE_SIZE.toString());
    });
  });

  describe('request.createReadChunkDoneResponse should create a response',
           function() {
    var readChunkDoneReponse;
    beforeEach(function() {
      readChunkDoneReponse = unpacker.request.createReadChunkDoneResponse(
          FILE_SYSTEM_ID, REQUEST_ID, CHUNK_BUFFER, CHUNK_OFFSET);
    });

    it('with READ_CHUNK_DONE as operation', function() {
      expect(readChunkDoneReponse[unpacker.request.Key.OPERATION])
          .to.equal(unpacker.request.Operation.READ_CHUNK_DONE);
    });

    it('with correct file system id', function() {
      expect(readChunkDoneReponse[unpacker.request.Key.FILE_SYSTEM_ID])
          .to.equal(FILE_SYSTEM_ID);
    });

    it('with correct request id', function() {
      expect(readChunkDoneReponse[unpacker.request.Key.REQUEST_ID])
          .to.equal(REQUEST_ID.toString());
    });

    it('with correct chunk buffer', function() {
      expect(readChunkDoneReponse[unpacker.request.Key.CHUNK_BUFFER])
          .to.equal(CHUNK_BUFFER);
    });

    it('with correct chunk offset', function() {
      expect(readChunkDoneReponse[unpacker.request.Key.OFFSET])
          .to.equal(CHUNK_OFFSET.toString());
    });
  });

  describe('request.createReadChunkErrorResponse should create a response',
           function() {
    var readChunkErrorReponse;
    beforeEach(function() {
      readChunkErrorReponse = unpacker.request.createReadChunkErrorResponse(
          FILE_SYSTEM_ID, REQUEST_ID, CHUNK_BUFFER);
    });

    it('with READ_CHUNK_ERROR as operation', function() {
      expect(readChunkErrorReponse[unpacker.request.Key.OPERATION])
          .to.equal(unpacker.request.Operation.READ_CHUNK_ERROR);
    });

    it('with correct file system id', function() {
      expect(readChunkErrorReponse[unpacker.request.Key.FILE_SYSTEM_ID])
          .to.equal(FILE_SYSTEM_ID);
    });

    it('with correct request id', function() {
      expect(readChunkErrorReponse[unpacker.request.Key.REQUEST_ID])
          .to.equal(REQUEST_ID.toString());
    });
  });

  describe('request.createCloseVolumeRequest should create a request',
           function() {
    var closeVolumeRequest;
    beforeEach(function() {
      closeVolumeRequest =
          unpacker.request.createCloseVolumeRequest(FILE_SYSTEM_ID);
    });

    it('with CLOSE_VOLUME as operation', function() {
      expect(closeVolumeRequest[unpacker.request.Key.OPERATION])
          .to.equal(unpacker.request.Operation.CLOSE_VOLUME);
    });

    it('with correct file system id', function() {
      expect(closeVolumeRequest[unpacker.request.Key.FILE_SYSTEM_ID])
          .to.equal(FILE_SYSTEM_ID);
    });

    it('with correct request id', function() {
      expect(closeVolumeRequest[unpacker.request.Key.REQUEST_ID])
          .to.equal(CLOSE_VOLUME_REQUEST_ID);
    });
  });

  describe('request.createOpenFileRequest should create a request', function() {
    var openFileRequest;
    beforeEach(function() {
      openFileRequest = unpacker.request.createOpenFileRequest(
          FILE_SYSTEM_ID, REQUEST_ID, INDEX, ENCODING, ARCHIVE_SIZE);
    });

    it('with OPEN_FILE as operation', function() {
      expect(openFileRequest[unpacker.request.Key.OPERATION])
          .to.equal(unpacker.request.Operation.OPEN_FILE);
    });

    it('with correct file system id', function() {
      expect(openFileRequest[unpacker.request.Key.FILE_SYSTEM_ID])
          .to.equal(FILE_SYSTEM_ID);
    });

    it('with correct request id', function() {
      expect(openFileRequest[unpacker.request.Key.REQUEST_ID])
          .to.equal(REQUEST_ID.toString());
    });

    it('with correct file path', function() {
      expect(openFileRequest[unpacker.request.Key.INDEX])
          .to.equal(INDEX.toString());
    });

    it('with correct encoding', function() {
      expect(openFileRequest[unpacker.request.Key.ENCODING]).to.equal(ENCODING);
    });

    it('with correct archive size', function() {
      expect(openFileRequest[unpacker.request.Key.ARCHIVE_SIZE])
          .to.equal(ARCHIVE_SIZE.toString());
    });
  });

  describe('request.createCloseFileRequest should create a request',
      function() {
    var closeFileRequest;
    beforeEach(function() {
      closeFileRequest = unpacker.request.createCloseFileRequest(
          FILE_SYSTEM_ID, REQUEST_ID, OPEN_REQUEST_ID);
    });

    it('with CLOSE_FILE as operation', function() {
      expect(closeFileRequest[unpacker.request.Key.OPERATION])
          .to.equal(unpacker.request.Operation.CLOSE_FILE);
    });

    it('with correct file system id', function() {
      expect(closeFileRequest[unpacker.request.Key.FILE_SYSTEM_ID])
          .to.equal(FILE_SYSTEM_ID);
    });

    it('with correct request id', function() {
      expect(closeFileRequest[unpacker.request.Key.REQUEST_ID])
          .to.equal(REQUEST_ID.toString());
    });

    it('with correct open request id', function() {
      expect(closeFileRequest[unpacker.request.Key.OPEN_REQUEST_ID])
          .to.equal(OPEN_REQUEST_ID.toString());
    });
  });

  describe('request.createReadFileRequest should create a request', function() {
    var readFileRequest;
    beforeEach(function() {
      readFileRequest = unpacker.request.createReadFileRequest(
          FILE_SYSTEM_ID, REQUEST_ID, OPEN_REQUEST_ID, OFFSET, LENGTH);
    });

    it('with READ_FILE as operation', function() {
      expect(readFileRequest[unpacker.request.Key.OPERATION])
          .to.equal(unpacker.request.Operation.READ_FILE);
    });

    it('with correct file system id', function() {
      expect(readFileRequest[unpacker.request.Key.FILE_SYSTEM_ID])
          .to.equal(FILE_SYSTEM_ID);
    });

    it('with correct request id', function() {
      expect(readFileRequest[unpacker.request.Key.REQUEST_ID])
          .to.equal(REQUEST_ID.toString());
    });

    it('with correct open request id', function() {
      expect(readFileRequest[unpacker.request.Key.OPEN_REQUEST_ID])
          .to.equal(OPEN_REQUEST_ID.toString());
    });

    it('with correct offset', function() {
      expect(readFileRequest[unpacker.request.Key.OFFSET])
          .to.equal(OFFSET.toString());
    });

    it('with correct length', function() {
      expect(readFileRequest[unpacker.request.Key.LENGTH])
          .to.equal(LENGTH.toString());
    });
  });
});
