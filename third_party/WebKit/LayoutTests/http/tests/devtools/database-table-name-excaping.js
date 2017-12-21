// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests how table names are escaped in database table view.\n`);
  await TestRunner.loadModule('application_test_runner');

  var tableName = 'table-name-with-dashes-and-"quotes"';
  var escapedTableName = Resources.DatabaseTableView.prototype._escapeTableName(tableName, '', true);
  TestRunner.addResult('Original value: ' + tableName);
  TestRunner.addResult('Escaped value: ' + escapedTableName);
  TestRunner.completeTest();
})();
