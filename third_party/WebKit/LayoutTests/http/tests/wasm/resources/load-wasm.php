<?php

    $fileName = "incrementer.wasm";
    $fileSize = filesize($fileName);

    header("Content-Type: " . "application/wasm");

    $fn = fopen($fileName, "rb");
    $buffer = fread($fn, $fileSize);
    print($buffer);
    flush();
    fclose($fn);

    exit;
?>
