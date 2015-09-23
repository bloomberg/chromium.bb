<?php
    header("Content-Security-Policy: suborigin foobar");
?>
<!DOCTYPE html>
<head>
</head>
<body>
    <script src="/js-test-resources/js-test.js"></script>
    <script>
        window.jsTestIsAsync = true;
        description("The test passes if 'window.onerror' gets sanitized information about an exception thrown in a script loaded from a suborigin.");

        window.onerror = function(msg, url, line, column, error) {
            window.msg = msg;
            window.url = url;
            window.line = line;
            window.column = column;
            window.errorObject = error;
            shouldBeFalse("/SomeError/.test(msg)");
            shouldBeEqualToString("url", "");
            shouldBe("line", "0");
            shouldBe("column", "0");
            shouldBe("window.errorObject", "null");
            finishJSTest();
        }
    </script>
    <script src="/security/resources/cors-script.php?fail=true&cors=false"></script>
</body>
</html>
