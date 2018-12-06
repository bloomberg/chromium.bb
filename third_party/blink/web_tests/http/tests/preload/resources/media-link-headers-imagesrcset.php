<?php
    header('Link: <http://127.0.0.1:8000/resources/square.png?large>; rel=preload; as=image; imagesrcset="http://127.0.0.1:8000/resources/square.png?small 300w, http://127.0.0.1:8000/resources/square.png?large 600w"; imagesizes="(min-width: 300px) and (max-width: 300px) 300px, 600px"', false);
?>
<!DOCTYPE html>
<meta name="viewport" content="width=300">
<script>
    window.addEventListener("load", function() {
        var entries = performance.getEntriesByType("resource");
        var smallLoaded = false;
        for (var i = 0; i < entries.length; ++i) {
            if (entries[i].name.indexOf("large") != -1)
                window.opener.postMessage("largeloaded", "*");
            if (entries[i].name.indexOf("small") != -1)
                smallLoaded = true;
        }
        if (smallLoaded)
            window.opener.postMessage("success", "*")
        window.opener.postMessage("smallnotloaded", "*");
    });
</script>
<script src="../resources/slow-script.pl?delay=200"></script>

