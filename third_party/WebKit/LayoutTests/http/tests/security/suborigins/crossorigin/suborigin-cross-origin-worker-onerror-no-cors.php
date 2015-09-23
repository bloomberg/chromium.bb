<?php
header("Content-Security-Policy: suborigin foobar");
?>
<!DOCTYPE html>
<html>
<head>
    <script>
        window.jsTestIsAsync = true;
        window.isOnErrorTest = true;
    </script>
    <script src="/js-test-resources/js-test.js"></script>
</head>
<body>
<script>
        description("Ensure that scripts imported into a Worker from cross-origin hosts trigger sanitized onerror messages.");

        var worker;
        shouldThrow('worker = new Worker("/workers/resources/worker-importscripts-onerror-sameorigin.js")', '"SecurityError: Failed to construct \'Worker\': Script at \'http://127.0.0.1:8000/workers/resources/worker-importscripts-onerror-sameorigin.js\' cannot be accessed from origin \'http://foobar_127.0.0.1:8000\'."');
        finishJSTest();
    </script>
</body>
</html>
