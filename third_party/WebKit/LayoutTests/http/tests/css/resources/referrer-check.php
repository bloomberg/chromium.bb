<?php

function getReferrerPath() {
    if (!isset($_SERVER["HTTP_REFERER"]))
        return "";
    $url = parse_url($_SERVER["HTTP_REFERER"]);
    return $url['path'];
}

function putImage() {
    $image = "../../resources/square100.png";
    header("Content-Type: image/png");
    header("Content-Length: " . filesize($image));
    header("Access-Control-Allow-Origin: *");
    ob_clean();
    flush();
    readfile($image);
}

function putFont() {
    $font = "../../resources/Ahem.ttf";
    header("Content-Type: font/truetype");
    header("Content-Length: " . filesize($font));
    header("Access-Control-Allow-Origin: *");
    ob_clean();
    flush();
    readfile($font);
}

$from = $_GET["from"];
$resource = $_GET["resource"];
$referrerPath = getReferrerPath();

$expectedReferrerPath = "/css/css-resources-referrer.html";
if ($from === "iframe") {
    $expectedReferrerPath = "/css/css-resources-referrer-srcdoc.html";
}

if ($referrerPath === $expectedReferrerPath) {
    if ($resource === "image" || $resource === "image2")
        putImage();
    else if ($resource === "font")
        putFont();
    else
        header("HTTP/1.1 500 Internal Server Error");
} else {
    header("HTTP/1.1 500 Internal Server Error");
}

?>
