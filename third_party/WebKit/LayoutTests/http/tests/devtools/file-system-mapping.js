// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests FileSystemMapping\n`);


  var paths = {
    FOO: 'file:///home/username/projects/foo',
    BAR: 'file:///home/username/projects/bar',
    BUILD: 'file:///home/username/project/build',
    SITE1: 'file:///www/site1'
  };

  function addFileSystem(fileSystemMapping, path) {
    TestRunner.addResult('Adding file system ' + path);
    fileSystemMapping._addFileSystemPath(path);
    checkAndDumpFileSystemMapping(fileSystemMapping);
  }

  function removeFileSystem(fileSystemMapping, path) {
    TestRunner.addResult('Removing file system ' + path);
    fileSystemMapping._removeFileSystemPath(path);
    checkAndDumpFileSystemMapping(fileSystemMapping);
  }

  function addFileMapping(fileSystemMapping, fileSystemPath, urlPrefix, pathPrefix) {
    TestRunner.addResult('Adding file mapping (' + fileSystemPath + ', ' + urlPrefix + ', ' + pathPrefix + ')');
    fileSystemMapping.addFileMapping(fileSystemPath, urlPrefix, pathPrefix);
    checkAndDumpFileSystemMapping(fileSystemMapping);
  }

  function removeFileMapping(fileSystemMapping, fileSystemPath, urlPrefix, pathPrefix) {
    TestRunner.addResult('Removing file mapping (' + fileSystemPath + ', ' + urlPrefix + ', ' + pathPrefix + ')');
    fileSystemMapping.removeFileMapping(fileSystemPath, urlPrefix, pathPrefix);
    checkAndDumpFileSystemMapping(fileSystemMapping);
  }

  function removeMappingForURL(fileSystemMapping, urlPrefix) {
    TestRunner.addResult('Removing file mapping for url ' + urlPrefix);
    fileSystemMapping.removeMappingForURL(urlPrefix);
    checkAndDumpFileSystemMapping(fileSystemMapping);
  }

  function addMappingForResource(fileSystemMapping, url, fileSystemPath, filePath) {
    TestRunner.addResult('Adding file mapping for resource (' + url + ', ' + fileSystemPath + ', ' + filePath + ')');
    fileSystemMapping.addMappingForResource(url, fileSystemPath, filePath);
    checkAndDumpFileSystemMapping(fileSystemMapping);
  }

  function dumpFileForURL(fileSystemMapping, url) {
    var hasMappingForNetworkURL = fileSystemMapping.hasMappingForNetworkURL(url);
    TestRunner.addResult('    Has mapping for \'' + url + '\': ' + hasMappingForNetworkURL);
    var fileForURL = fileSystemMapping.fileForURL(url);
    if (!fileForURL)
      TestRunner.addResult('    File for \'' + url + '\': null');
    else
      TestRunner.addResult('    File for \'' + url + '\': ' + fileForURL.fileURL);
  }

  function dumpURLForPath(fileSystemMapping, fileSystemPath, filePath) {
    var url = fileSystemMapping.networkURLForFileSystemURL(fileSystemPath, filePath);
    TestRunner.addResult('    URL for path \'' + filePath + '\': ' + url);
  }

  function checkAndDumpFileSystemMapping(fileSystemMapping) {
    var fileSystemPaths = Object.keys(fileSystemMapping._fileSystemMappings);
    TestRunner.addResult('Testing file system mapping.');
    TestRunner.addResult('    file system paths:');
    for (var i = 0; i < fileSystemPaths.length; ++i) {
      TestRunner.addResult('     - ' + fileSystemPaths[i]);
      var entries = fileSystemMapping.mappingEntries(fileSystemPaths[i]);
      for (var j = 0; j < entries.length; ++j) {
        var entry = entries[j];
        TestRunner.addResult('         - ' + JSON.stringify(entries[j]));
      }
    }
    TestRunner.addResult('');
  }

  // At first create file system mapping and clear it.
  var fileSystemMapping = new Persistence.FileSystemMapping(Persistence.isolatedFileSystemManager);
  var fileSystemPaths = Object.keys(fileSystemMapping._fileSystemMappings);
  for (var i = 0; i < fileSystemPaths.length; ++i)
    fileSystemMapping._removeFileSystemPath(fileSystemPaths[i]);

  // Now fill it with file systems.
  checkAndDumpFileSystemMapping(fileSystemMapping);
  addFileSystem(fileSystemMapping, paths.FOO);
  addFileSystem(fileSystemMapping, paths.BAR);
  addFileSystem(fileSystemMapping, paths.BUILD);
  addFileSystem(fileSystemMapping, paths.SITE1);

  // Now fill it with file mappings.
  addFileMapping(fileSystemMapping, paths.SITE1, 'http://localhost/', '/');
  addFileMapping(fileSystemMapping, paths.SITE1, 'http://www.foo.com/', '/foo/');
  addFileMapping(fileSystemMapping, paths.FOO, 'http://www.example.com/bar/', '/foo/');
  addMappingForResource(
      fileSystemMapping, 'http://www.bar.com/foo/folder/42.js', paths.FOO, paths.FOO + '/baz/folder/42.js');
  addMappingForResource(
      fileSystemMapping, 'http://localhost:3333/build/layout.css', paths.BUILD, paths.BUILD + '/layout.css');

  TestRunner.addResult('Testing mappings for url:');
  dumpFileForURL(fileSystemMapping, 'http://www.bar.com/foo/folder/42.js');
  dumpFileForURL(fileSystemMapping, 'http://www.foo.com/bar/folder/42.js');
  dumpFileForURL(fileSystemMapping, 'http://localhost/index.html');
  dumpFileForURL(fileSystemMapping, 'https://localhost/index.html');
  dumpFileForURL(fileSystemMapping, 'http://localhost:8080/index.html');
  dumpFileForURL(fileSystemMapping, 'http://localhost/');
  dumpFileForURL(fileSystemMapping, 'http://localhost:3333/build/main.css');
  TestRunner.addResult('');

  TestRunner.addResult('Testing mappings for path:');
  dumpURLForPath(fileSystemMapping, paths.FOO, paths.FOO + '/baz/folder/42.js');
  dumpURLForPath(fileSystemMapping, paths.FOO, paths.FOO + '/baz/folder/43.js');
  dumpURLForPath(fileSystemMapping, paths.FOO, paths.FOO + '/bar/folder/42.js');
  dumpURLForPath(fileSystemMapping, paths.FOO, paths.FOO + '/foo/folder/42.js');
  dumpURLForPath(fileSystemMapping, paths.FOO, paths.FOO + '/foo2/folder/42.js');
  dumpURLForPath(fileSystemMapping, paths.SITE1, paths.SITE1 + '/foo/index.html');
  dumpURLForPath(fileSystemMapping, paths.SITE1, paths.SITE1 + '/index.html');
  dumpURLForPath(fileSystemMapping, paths.SITE1, paths.SITE1 + '/foo');
  dumpURLForPath(fileSystemMapping, paths.SITE1, paths.SITE1 + '/foo/');
  TestRunner.addResult('');

  // Then create another file mapping to make sure it is correctly restored from the settings.
  TestRunner.addResult('Creating another file system mapping.');
  fileSystemMapping.dispose();
  var fileSystemMapping = new Persistence.FileSystemMapping(Persistence.isolatedFileSystemManager);
  checkAndDumpFileSystemMapping(fileSystemMapping);

  // Now remove file mappings.
  removeMappingForURL(fileSystemMapping, 'http://www.bar.com/foo/folder/42.js');
  removeFileMapping(fileSystemMapping, paths.SITE1, 'http://localhost/', '/');
  removeFileMapping(fileSystemMapping, paths.SITE1, 'http://www.foo.com/', '/foo/');
  removeFileMapping(fileSystemMapping, paths.FOO, 'http://www.example.com/bar/', '/foo/');

  // Now remove file systems.
  removeFileSystem(fileSystemMapping, paths.SITE1);
  removeFileSystem(fileSystemMapping, paths.FOO);
  removeFileSystem(fileSystemMapping, paths.BAR);

  TestRunner.completeTest();
})();
