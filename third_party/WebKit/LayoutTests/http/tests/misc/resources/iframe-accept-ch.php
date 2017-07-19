<?php
    header("ACCEPT-CH: DPR, Width, Viewport-Width, Device-Memory");
?>
<!DOCTYPE html>
<body>
    <script>
        var fail = function(num) {
            return function() {
                parent.postMessage("fail "+ num, "*");
            }
        };

        var success = function() {
            parent.postMessage("success", "*");
        };

        var loadDeviceMemoryImage = function() {
            var img = new Image();
            img.src = 'image-checks-for-device-memory.php';
            img.onload = success;
            img.onerror = fail(4);
            document.body.appendChild(img);
        };
        var loadRWImage = function() {
            var img = new Image();
            img.src = 'image-checks-for-width.php';
            img.sizes = '500';
            img.onload = loadDeviceMemoryImage;
            img.onerror = fail(3);
            document.body.appendChild(img);
        };
        var loadViewportImage = function() {
            var img = new Image();
            img.src = 'image-checks-for-viewport-width.php';
            img.onload = loadRWImage;
            img.onerror = fail(2);
            document.body.appendChild(img);
        };
        var img = new Image();
        img.src = 'image-checks-for-dpr.php';
        img.onload = loadViewportImage;
        img.onerror = fail(1);
        document.body.appendChild(img);
    </script>
</body>
