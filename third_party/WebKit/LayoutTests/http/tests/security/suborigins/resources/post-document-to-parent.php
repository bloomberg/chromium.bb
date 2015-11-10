<?php
if ($_GET["suborigin"]) {
    header("Content-Security-Policy: suborigin ". $_GET["suborigin"]);
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
