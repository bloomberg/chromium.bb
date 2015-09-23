<?php
    $date = $_GET['date'];
    $expected = $_GET['expected'];
    header("Last-Modified: $date");
?>
<script>
var expected = '<?=$expected?>';
var lastModified = document.lastModified;
if (expected == lastModified) {
    document.write('PASS');
} else {
    document.write('FAIL: expect ' + expected + ', but got ' + lastModified);
}
</script>
