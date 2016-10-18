<?php
    header("Content-Security-Policy: script-src 'self' 'nonce-abc'");
?>
<!doctype html>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script nonce="abc">
    var t = async_test("Unnonced scripts generate reports.");
    var events = 0;
    var expectations = {
        37: true,
        40: true,
        43: true,
        46: true,
        49: true,
        "https://evil.example.test/yay1.js": true,
        "https://evil.example.test/yay2.js": true,
        "https://evil.example.test/yay3.js": true,
        "https://evil.example.test/yay4.js": true
    };

    document.addEventListener('securitypolicyviolation', t.step_func(e => {
        if (e.lineNumber) {
            // Verify that the line is expected, then clear the expectation:
            assert_true(expectations[e.lineNumber]);
            expectations[e.lineNumber] = false;
            assert_equals(e.blockedURI, "inline");
        } else {
            // Otherwise, verify that the URL is expected, then clear the expectation:
            assert_true(expectations[e.blockedURI]);
            expectations[e.blockedURI] = false;
        }
        events++;console.log(events + " : " + e.lineNumber + " : " + e.blockedURI);
        if (events == 9)
            t.done();
    }));

    var unexecuted_test = async_test("Blocks without correct nonce do not execute, and generate violation reports");
</script>
<script>
    unexecuted_test.unreached_func("This code block should not execute.")();
</script>
<script nonce="xyz">
    unexecuted_test.unreached_func("This code block should not execute.")();
</script>
<script <script nonce="abc">
    unexecuted_test.unreached_func("This code block should not execute.")();
</script>
<script attribute<script nonce="abc">
    unexecuted_test.unreached_func("This code block should not execute.")();
</script>
<script attribute=<script nonce="abc">
    unexecuted_test.unreached_func("This code block should not execute.")();
</script>
<script src=https://evil.example.test/yay1.js <script nonce="abc">
    unexecuted_test.unreached_func("This code block should not execute.")();
</script>
<script src=https://evil.example.test/yay2.js attribute=<script nonce="abc">
    unexecuted_test.unreached_func("This code block should not execute.")();
</script>
<script src=https://evil.example.test/yay3.js <style nonce="abc">
    unexecuted_test.unreached_func("This code block should not execute.")();
</style></script>
<script src=https://evil.example.test/yay4.js attribute=<style nonce="abc">
    unexecuted_test.unreached_func("This code block should not execute.")();
</style></script>
<script nonce="abc">
    unexecuted_test.done();
</script>
