<script>
mainTestWindow = window.open("", "mainTestWindow")
var data = {
    'method': '<?=$_SERVER['REQUEST_METHOD'] ?>',
    'formValue': '<?= $_POST['a'] ?>',
};
mainTestWindow.postMessage(data, '*');
</script>
