<?php
header("Content-Security-Policy: script-src 'nonce-abcd1234'");
header("Content-Security-Policy-Report-Only: script-src 'self'");
?>
<!DOCTYPE html>
<script src="/resources/testharness.js" nonce="abcd1234"></script>
<script src="/resources/testharnessreport.js" nonce="abcd1234"></script>
<script>
    assert_unreached("This script block has no nonce, and should not execute.");
</script>
<script nonce="abcd1234">
    test(_ => {
      assert_true(true);
    }, "This script block has a matching nonce, and should execute.");  
</script>
