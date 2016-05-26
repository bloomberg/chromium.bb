<?php
  header("Content-Security-Policy-Report-Only: block-all-mixed-content");
?>
<!doctype html>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<body>
<script>
    async_test(t => {
        var i = document.createElement('img');
        i.onerror = t.assert_unreached;
        i.onload = t.step_func_done(_ => {
            assert_equals(128, i.naturalWidth);
            assert_equals(128, i.naturalHeight);
        });
        i.src = "http://127.0.0.1:8080/security/resources/compass.jpg?t=1";
    }, "Mixed images are allowed in the presence of 'block-all-mixed-content' in report-only mode.");

    async_test(t => {
        var i = document.createElement('img');
        document.addEventListener('securitypolicyviolation', t.step_func_done(e => {
            var expectations = {
                'documentURI': document.location.toString(),
                'referrer': document.referrer,
                'blockedURI': 'http://127.0.0.1:8080',
                'violatedDirective': 'block-all-mixed-content',
                'effectiveDirective': 'block-all-mixed-content',
                'originalPolicy': 'block-all-mixed-content',
                'sourceFile': '',
                'lineNumber': 0,
                'columnNumber': 0,
                'statusCode': 0
            };
            for (key in expectations)
                assert_equals(expectations[key], e[key], key);
        }));
        i.src = "http://127.0.0.1:8080/security/resources/compass.jpg?t=2";
    }, "Mixed images generate CSP violation reports in the presence of 'block-all-mixed-content'.");
</script>
