<?php
    $rw = $_SERVER["HTTP_RW"];

    if($rw) {
        $fn = fopen("compass.jpg", "r");
        fpassthru($fn);
        fclose($fn);
        exit;
    }
    header("HTTP/1.1 417 Expectation failed");
?>
