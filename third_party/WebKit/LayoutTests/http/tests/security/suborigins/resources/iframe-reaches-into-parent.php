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
  window.parent.postMessage(secret, '*');
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
