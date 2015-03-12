<?php
if (strtolower($_GET["cors"]) != "false") {
    header("Access-Control-Allow-Origin: http://127.0.0.1:8000");
}
if (strtolower($_GET["credentials"]) == "true") {
    header("Access-Control-Allow-Credentials: true");
}
header("Content-Type: text/css");
echo ".id1 { background-color: yellow }";
?>
