<?php
header("Suborigin: foobar");
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<script>
window.jsTestIsAsync = true;
window.isOnErrorTest = true;
</script>
<script src="/js-test-resources/js-test.js"></script>
</head>
<body>
<script>
description('Ensure that scripts imported into a Worker from cross-origin hosts trigger sanitized onerror messages.');

var worker;
var worker_path =
  '/workers/resources/worker-importscripts-onerror-sameorigin.js';
shouldThrow(
  'worker = new Worker("' + worker_path + '")',
  '"SecurityError: Failed to construct \'Worker\': Script at ' +
  '\'http://127.0.0.1:8000' + worker_path +
  '\' cannot be accessed from origin \'http-so://foobar.127.0.0.1:8000\'."');
finishJSTest();
</script>
</body>
</html>
