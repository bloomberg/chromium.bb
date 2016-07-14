<?php
    header("Content-Security-Policy: script-src 'nonce-abc'");
?>
<!doctype html>
<script nonce="abc" src="/resources/testharness.js"></script>
<script nonce="abc" src="/resources/testharnessreport.js"></script>
<script nonce="abc">
    async_test(t => {
        window.onload = t.step_func(_ => {
            var link = document.createElement('link');
            link.setAttribute("rel", "import");
            link.setAttribute("nonce", "zyx");
            link.setAttribute("href", "/security/resources/blank.html");

            var watcher = new EventWatcher(t, document, ['securitypolicyviolation']);
            watcher
                .wait_for('securitypolicyviolation')
                .then(t.step_func_done(e => {
                    assert_false(!!link.import, "The import was not loaded.");
                    assert_equals(e.blockedURI, "http://127.0.0.1:8000/security/resources/blank.html");
                    assert_equals(e.lineNumber, 21);
                 }));

            document.body.appendChild(link);
        });
    }, "Incorrectly nonced imports do not load, and trigger reports.");
</script>
