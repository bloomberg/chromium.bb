<?php
$suborigin = $_GET["suborigin"];
$suboriginPolicy = $_GET["suboriginpolicy"];

if ($_GET["suborigin"]) {
    header("Suborigin: " . $suborigin . " " . $suboriginPolicy);
}
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
</head>
<script>
var type = '<?php echo $_GET["type"]; ?>';
var is_iframe = (type === 'iframe');
var p = window.opener;
if (is_iframe)
  p = window.parent;

var target = '<?php if ($_GET["target"]) echo $_GET["target"]; else echo "*"; ?>';
p.postMessage({
    'suborigin': document.suborigin,
    'type': type
  }, target);
</script>
</html>
