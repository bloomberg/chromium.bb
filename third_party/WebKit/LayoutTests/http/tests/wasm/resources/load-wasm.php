<?php

    $fileName = "incrementer.wasm";
    if (isset($_GET['name'])) {
       $fileName = $_GET['name'];
    }
    $fileSize = filesize($fileName);

    header("Content-Type: " . "application/wasm");

    $fn = fopen($fileName, "rb");
    $buffer = fread($fn, $fileSize);
    print($buffer);
    flush();
    fclose($fn);

    exit;
?>
