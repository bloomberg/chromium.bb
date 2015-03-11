<?php
if (strtolower($_GET["cors"]) != "false") {
    header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
}
if (strtolower($_GET["credentials"]) == "true") {
    header("Access-Control-Allow-Credentials: true");
}
header("Content-Type: application/javascript");
$delay = $_GET['delay'];
if ($delay)
    usleep(1000 * $delay);
$value = $_GET['value'];
if ($_SERVER['HTTP_ORIGIN'] && $_GET['value_cors']) {
    $value = $_GET['value_cors'];
}
if ($value)
    echo "result = \"" . $value . "\";";
else if (strtolower($_GET["fail"]) == "true")
    echo "throw({toString: function(){ return 'SomeError' }});";
else
    echo "alert('script ran.');";
?>
