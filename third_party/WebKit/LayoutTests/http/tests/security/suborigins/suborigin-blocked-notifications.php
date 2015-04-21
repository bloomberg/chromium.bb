<?php
header("Content-Security-Policy: suborigin foobar");
?>
<!DOCTYPE html>
<html>
<head>
<title>Notifications are denied in suborigins.</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<script>
  testRunner.grantWebNotificationPermission(location.origin, true);
  Notification.requestPermission(function (status) {
      assert_equals(status, 'default', 'The notification permission request should not be granted.');

      var notification = new Notification('My Notification');
      notification.addEventListener('show', function() {
          assert_unreached('The notification is not expected to be shown.');
      });

      notification.addEventListener('error', function() {
          done();
      });
  });
</script>
</html>
