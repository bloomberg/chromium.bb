<?php
header("Content-Type: text/html; charset=UTF-8");
session_start();
?>
<html>
<title>iframe</title>
<body>
<script>
<?php
if (!isset($_SESSION['count'])) {
    $_SESSION['count'] = 0;
} else {
    $_SESSION['count']++;
}
echo "var a = '";
echo $_SESSION['count'];
echo "';";
?>
console.log(a);
</script>
<div>test</div>
</body>
</html>

