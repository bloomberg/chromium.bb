// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupTests(tester, plugin) {
  function addTest(testName, responses) {
    var expectedMessages = [testName + ':PASSED'];
    if (responses != undefined)
      expectedMessages = expectedMessages.concat(responses);
    tester.addAsyncTest('PPB_FileIO::' + testName, function(test) {
      test.expectMessageSequence(plugin, expectedMessages);
      plugin.postMessage(testName);
    });
  }
  var setupCallbacks = [
      'OpenFileSystemForSetupCallback',
      'OpenFileForSetupCallback',
      'WriteFileForSetupCallback',
      'FlushFileForSetupCallback',
      'TouchFileForSetupCallback'];
  addTest('TestOpenExistingFileLocalPersistent',
          setupCallbacks.concat(['TestOpenForRead',
                                 'TestOpenForWrite',
                                 'TestOpenTruncate',
                                 'TestOpenForWriteCreateExclusive',
                                 'TestOpenForWriteCreate',
                                 'END']));
  addTest('TestOpenExistingFileLocalTemporary',
          setupCallbacks.concat(['TestOpenForRead',
                                 'TestOpenForWrite',
                                 'TestOpenTruncate',
                                 'TestOpenForWriteCreateExclusive',
                                 'TestOpenForWriteCreate',
                                 'END']));
  addTest('TestOpenNonExistingFileLocalPersistent',
          setupCallbacks.concat(['TestOpenForRead',
                                 'TestOpenForWrite',
                                 'TestOpenForWriteCreate',
                                 'DeleteFile',
                                 'TestOpenForWriteCreateExclusive',
                                 'END']));
  addTest('TestOpenNonExistingFileLocalTemporary',
          setupCallbacks.concat(['TestOpenForRead',
                                 'TestOpenForWrite',
                                 'TestOpenForWriteCreate',
                                 'DeleteFile',
                                 'TestOpenForWriteCreateExclusive',
                                 'END']));
  addTest('TestQueryFileLocalPersistent',
          setupCallbacks.concat(['OpenFileForTest',
                                 'TestQuery',
                                 'TestQueryFileVerify',
                                 'END']));
  addTest('TestQueryFileLocalTemporary',
          setupCallbacks.concat(['OpenFileForTest',
                                 'TestQuery',
                                 'TestQueryFileVerify',
                                 'END']));
  addTest('TestPartialFileReadLocalPersistent',
          setupCallbacks.concat(['OpenFileForTest',
                                 'TestFileRead',
                                 'ReadCallback:VERIFIED',
                                 'END']));
  addTest('TestPartialFileReadLocalTemporary',
          setupCallbacks.concat(['OpenFileForTest',
                                 'TestFileRead',
                                 'ReadCallback:VERIFIED',
                                 'END']));
  addTest('TestCompleteReadLocalPersistent',
          setupCallbacks.concat(['OpenFileForTest',
                                 'TestFileRead',
                                 'ReadCallback:VERIFIED',
                                 'END']));
  addTest('TestCompleteReadLocalTemporary',
          setupCallbacks.concat(['OpenFileForTest',
                                 'TestFileRead',
                                 'ReadCallback:VERIFIED',
                                 'END']));
  addTest('TestParallelReadLocalPersistent',
          setupCallbacks.concat(['OpenFileForTest',
                                 'TestParallelRead',
                                 'ReadCallback:VERIFIED',
                                 'ReadCallback:VERIFIED',
                                 'END']));
  addTest('TestParallelReadLocalTemporary',
          setupCallbacks.concat(['OpenFileForTest',
                                 'TestParallelRead',
                                 'ReadCallback:VERIFIED',
                                 'ReadCallback:VERIFIED',
                                 'END']));
};
