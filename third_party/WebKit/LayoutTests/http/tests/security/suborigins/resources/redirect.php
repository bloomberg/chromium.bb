<?php
$url = $_GET['url'];
header("Location: $url");

$code = $_GET['code'];
if (!isset($code))
    $code = 302;
header("HTTP/1.1 $code");

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

$suborigin_arg = strtolower($_GET["suborigin"]);
if (!(empty($suborigin_arg))) {
    header("Access-Control-Allow-Suborigin: " . $suborigin_arg);
}

if ($_SERVER["HTTP_SUBORIGIN"] == "foobar") {
    header("Access-Control-Allow-Suborigin: foobar");
}
?>
