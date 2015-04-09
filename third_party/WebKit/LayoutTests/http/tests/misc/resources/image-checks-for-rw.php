<?php
    $rw = $_SERVER["HTTP_RW"];
    $expected_rw = $_GET["rw"];

    if (($rw == $expected_rw) || ($rw && !$expected_rw)) {
        $fn = fopen("compass.jpg", "r");
        fpassthru($fn);
        fclose($fn);
        exit;
    }
    header("HTTP/1.1 417 Expectation failed");
?>
