<?php
    header("Link: <http://127.0.0.1:8000/resources/square.png>;rel=preload;as=image;", false);
?>
<!DOCTYPE html>
<script>
    window.addEventListener("load", function() {
        var entries = performance.getEntriesByType("resource");
        var smallLoaded = false;
        for (var i = 0; i < entries.length; ++i) {
            top.console.log(entries[i].name);
            if (entries[i].name.indexOf("square") != -1)
                top.postMessage("squareloaded", "*");
        }
        top.postMessage("notloaded", "*");
    });
</script>
<script src="../resources/slow-script.pl?delay=200"></script>

