<?php
$cors_arg = strtolower($_GET["cors"]);
if ($cors_arg != "false") {
    if ($cors_arg == "" || $cors_arg == "true") {
        header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
    } else {
        header("Access-Control-Allow-Origin: " . $cors_arg . "");
    }
}
if (strtolower($_GET["credentials"]) == "true") {
    header("Access-Control-Allow-Credentials: true");
}
$custom_header_arg = strtolower($_GET["custom"]);
if (!(empty($custom_header_arg))) {
    header("Access-Control-Allow-Headers: " . $custom_header_arg);
}

$suborigin_arg = strtolower($_GET["suborigin"]);
if (!(empty($suborigin_arg))) {
    header("Access-Control-Allow-Suborigin: " . $suborigin_arg); 
}

if ($_SERVER["HTTP_SUBORIGIN"] == "foobar") {
    header("Access-Control-Allow-Suborigin: foobar");
}

header("Content-Type: application/javascript");
$delay = $_GET['delay'];
if ($delay)
    usleep(1000 * $delay);
$value = $_GET['value'];
$result_var = "result";
if (!empty($_GET["resultvar"])) {
  $result_var = $_GET["resultvar"];
}
$cookie = $_GET['cookie'];
if ($_SERVER['HTTP_ORIGIN'] && $_GET['value_cors']) {
    $value = $_GET['value_cors'];
}
if ($value || $cookie) {
    if ($cookie) {
      $value = $_COOKIE[$cookie];
    }

    echo $result_var . " = \"" . $value . "\";";
} else if (strtolower($_GET["fail"]) == "true")
    echo "throw({toString: function(){ return 'SomeError' }});";
else
    echo "alert('script ran.');";
?>
