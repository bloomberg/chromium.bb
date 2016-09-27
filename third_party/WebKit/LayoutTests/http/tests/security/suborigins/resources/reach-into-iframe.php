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
window.secret = '';
window.onmessage = function() {
  var iframe = document.getElementById('iframe');
  var secret;
  try {
    secret = iframe.contentWindow.secret;
  } catch (e) {
    secret = e.toString();
  }
  parent.postMessage(secret, '*');
};
</script>
<?php
if ($_GET["childsuborigin"]) {
    echo "<iframe id=\"iframe\" src=\"post-to-parent.php?suborigin=" .
         $_GET["childsuborigin"] . "\"></iframe>";
} else {
    echo "<iframe id=\"iframe\" src=\"post-to-parent.php\"></iframe>";
}
?>
</html>
