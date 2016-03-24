<?php
    header("Link: <http://127.0.0.1:8000/resources/square.png>;rel=preload;as=image;", false);
?>
<!DOCTYPE html>
<script>
    if (window.internals) {
        if (internals.isPreloaded("http://127.0.0.1:8000/resources/square.png"))
            top.postMessage("squareloaded", "*");
        else
            top.postMessage("notloaded", "*");
    }
</script>

