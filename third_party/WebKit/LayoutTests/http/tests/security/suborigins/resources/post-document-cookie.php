<!DOCTYPE html>
<html>
<script>
var test_name = '<?php echo $_GET["testname"]; ?>';
var data = {
    cookie_val: document.cookie,
    test_name: test_name
};
window.parent.postMessage(data, '*');
</script>
</html>
