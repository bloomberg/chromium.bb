<?php
    header("Content-Security-Policy: script-src 'self' 'nonce-abc'");
?>
<!doctype html>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script nonce="abc">
    async_test(t => {
        var watcher = new EventWatcher(t, document, ['securitypolicyviolation', 'securitypolicyviolation','securitypolicyviolation', 'securitypolicyviolation','securitypolicyviolation','securitypolicyviolation', 'securitypolicyviolation', 'securitypolicyviolation']);
        watcher
            .wait_for('securitypolicyviolation')
            .then(t.step_func(e => {
                assert_equals(e.blockedURI, "inline");
                assert_equals(e.lineNumber, 58);
                return watcher.wait_for('securitypolicyviolation');
            }))
            .then(t.step_func(e => {
                assert_equals(e.blockedURI, "inline");
                assert_equals(e.lineNumber, 61);
                return watcher.wait_for('securitypolicyviolation');
            }))
            .then(t.step_func(e => {
                assert_equals(e.blockedURI, "inline");
                assert_equals(e.lineNumber, 64);
                return watcher.wait_for('securitypolicyviolation');
            }))
            .then(t.step_func(e => {
                assert_equals(e.blockedURI, "inline");
                assert_equals(e.lineNumber, 67);
                return watcher.wait_for('securitypolicyviolation');
            }))
            .then(t.step_func(e => {
                assert_equals(e.blockedURI, "inline");
                assert_equals(e.lineNumber, 70);
                return watcher.wait_for('securitypolicyviolation');
            }))
            .then(t.step_func(e => {
                assert_equals(e.blockedURI, "https://evil.example.test/yay1.js");
                assert_equals(e.lineNumber, 0);
                return watcher.wait_for('securitypolicyviolation');
            }))
            .then(t.step_func(e => {
                assert_equals(e.blockedURI, "https://evil.example.test/yay2.js");
                assert_equals(e.lineNumber, 0);
                return watcher.wait_for('securitypolicyviolation');
            }))
            .then(t.step_func(e => {
                assert_equals(e.blockedURI, "https://evil.example.test/yay3.js");
                assert_equals(e.lineNumber, 0);
                return watcher.wait_for('securitypolicyviolation');
            }))
            .then(t.step_func_done(e => {
                assert_equals(e.blockedURI, "https://evil.example.test/yay4.js");
                assert_equals(e.lineNumber, 0);
            }));
    }, "Unnonced script blocks generate reports.");

    var executed_test = async_test("Nonced script executes, and does not generate a violation report.");
    var unexecuted_test = async_test("Blocks without correct nonce do not execute, and generate violation reports");
</script>
<script>
    unexecuted_test.assert_unreached("This code block should not execute.");
</script>
<script nonce="xyz">
    unexecuted_test.assert_unreached("This code block should not execute.");
</script>
<script <script nonce="abc">
    unexecuted_test.assert_unreached("This code block should not execute.");
</script>
<script attribute<script nonce="abc">
    unexecuted_test.assert_unreached("This code block should not execute.");
</script>
<script attribute=<script nonce="abc">
    unexecuted_test.assert_unreached("This code block should not execute.");
</script>
<script src=https://evil.example.test/yay1.js <script nonce="abc">
    unexecuted_test.assert_unreached("This code block should not execute.");
</script>
<script src=https://evil.example.test/yay2.js attribute=<script nonce="abc">
    unexecuted_test.assert_unreached("This code block should not execute.");
</script>
<script src=https://evil.example.test/yay3.js <style nonce="abc">
    unexecuted_test.assert_unreached("This code block should not execute.");
</style></script>
<script src=https://evil.example.test/yay4.js attribute=<style nonce="abc">
    unexecuted_test.assert_unreached("This code block should not execute.");
</style></script>
<script nonce="abc">
    executed_test.done();
    unexecuted_test.done();
</script>
