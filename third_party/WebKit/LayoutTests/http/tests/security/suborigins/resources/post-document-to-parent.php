<?php
$suborigin = $_GET["suborigin"];
$suboriginPolicy = $_GET["suboriginpolicy"];

if ($_GET["suborigin"]) {
    header("Suborigin: " . $suborigin . " " . $suboriginPolicy);
}
?>
<!DOCTYPE html>
<html>
<script>
var type = '<?php echo $_GET["type"]; ?>';
var is_iframe = (type === 'iframe');
var p = window.opener;
if (is_iframe)
    p = window.parent;

p.postMessage({
  'suborigin': document.suborigin,
  'type': type
}, '*');
</script>
</html>
