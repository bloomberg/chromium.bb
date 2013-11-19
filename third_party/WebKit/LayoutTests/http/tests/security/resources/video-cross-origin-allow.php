<?php

if (isset($_GET['no-preflight']) && $_SERVER['REQUEST_METHOD'] == "OPTIONS") {
    echo "FAIL";
    exit;
}

header("Access-Control-Allow-Origin: *");

if ($_SERVER['REQUEST_METHOD'] == "OPTIONS") {
    header("Access-Control-Allow-Methods: GET");
    header("Access-Control-Allow-Headers: origin, accept-encoding, referer, range");
    exit;
}

@include("../../media/resources/serve-video.php");

?>
