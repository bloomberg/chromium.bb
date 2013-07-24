<script>
if (window.opener) {
    var data = {
        'method': '<?=$_SERVER['REQUEST_METHOD'] ?>',
        'formValue': '<?= $_POST['a'] ?>',
    };
    window.opener.postMessage(data, '*');
} else {
    document.writeln('FAIL');
    testRunner.notifyDone();
}
</script>
