<?php
header("Suborigin: foobar");
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Scripts imported into a worker from crossorigin hosts throw SecurityError</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<body>
<script>
var worker;
var worker_path =
    '/workers/resources/worker-importscripts-onerror-sameorigin.js';
assert_throws('SecurityError', function() {
    worker = new Worker(worker_path);
  });
done();
</script>
</body>
</html>
