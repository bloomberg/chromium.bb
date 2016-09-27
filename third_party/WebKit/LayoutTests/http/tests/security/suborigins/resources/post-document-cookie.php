<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
</head>
<script>
var test_name = '<?php echo $_GET["testname"]; ?>';
var data = {
  'cookie_val': document.cookie,
  'test_name': test_name
};
window.parent.postMessage(data, '*');
</script>
</html>
