<?php
    header("Content-Security-Policy: style-src 'nonce-abc'");
?>
<!doctype html>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<style nonce="abc">
    #test1 {
        color: rgba(1,1,1,1);
    }
</style>
<link rel="stylesheet" href="/security/contentSecurityPolicy/style-set-red.css" nonce="abc">
<script>
    async_test(t => {
        document.addEventListener('securitypolicyviolation', t.unreached_func("No report should be generated"));
        window.onload = t.step_func_done(_ => {
            assert_equals(document.styleSheets.length, 2);
            assert_equals(document.styleSheets[0].href, null);
            assert_equals(document.styleSheets[1].href, "http://127.0.0.1:8000/security/contentSecurityPolicy/style-set-red.css");
        });
    }, "Nonced stylesheets load, and do not trigger reports.");
</script>
