<?php

    $fileName = $_GET["name"];
    $type = $_GET["type"];
    $norange = $_GET["norange"];

    $_GET = array();
    $_GET['name'] = $fileName;
    $_GET['type'] = $type;
    $_GET['norange'] = $norange;
    @include("./serve-video.php");

?>
