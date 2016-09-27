<?php
if ($_GET["suborigin"]) {
    header("Suborigin: " . $_GET["suborigin"]);
}
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
</head>
<script>
window.secret = 'I am a secret';
try {
  window.parent.secret = 'I am a secret';
} catch(e) {
  // Ignore. The fact that secret hasn't changed in the parent will be
  // recognized in the parent.
}
window.parent.postMessage('Done', '*');
</script>
</html>
