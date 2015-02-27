<?php
    header("ACCEPT-CH: DPRW");
?>
<!DOCTYPE html>
<script src="../resources/testharness.js"></script>
<script src="../resources/testharnessreport.js"></script>
<body>
    <script>
        var t = async_test('Client-Hints sent when Accept-CH header is present');
        var unreached = function() {
            assert_unreached("Image should have loaded.");
        };

        var loadRWImage = function() {
            var img = new Image();
            img.src = 'resources/image-checks-for-rw.php';
            img.onload = t.step_func(unreached);
            img.onerror = t.step_func(function(){ t.done(); });
            document.body.appendChild(img);
        };
        t.step(function() {
            var img = new Image();
            img.src = 'resources/image-checks-for-dpr.php';
            img.onload = t.step_func(unreached);
            img.onerror = t.step_func(loadRWImage);
            document.body.appendChild(img);
        });
    </script>
</body>
