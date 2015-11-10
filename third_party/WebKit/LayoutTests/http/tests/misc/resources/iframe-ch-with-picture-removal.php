<?php
    header("ACCEPT-CH: DPR, Width, Viewport-Width");
?>
<!DOCTYPE html>
<body>
<script>
    window.addEventListener("message", function (message) {
        var pic = document.getElementById("pic");
        pic.removeChild(pic.childNodes[0]);
        // TODO(yoav): this should trigger a load, but doesn't. See https://crbug.com/418903
        success();
    });

    var fail = function(num) {
        parent.postMessage("fail "+ num, "*");
    };

    var success = function() {
        parent.postMessage("success", "*");
    };

    var remove = function() {
        parent.postMessage("remove", "*");
    };

    var counter = 1;
    var error = function() {
        fail(counter);
    }
    var load = function() {
        if (counter == 1) {
            ++counter;
            remove();
            return;
        }
        success();
    }
</script>
<picture id=pic>
    <source sizes="50vw" media="(min-width: 800px)" srcset="image-checks-for-width.php?rw=400">
    <source sizes="50vw" srcset="image-checks-for-width.php?rw=300">
    <img onerror="error()" onload="load()">
</picture>
