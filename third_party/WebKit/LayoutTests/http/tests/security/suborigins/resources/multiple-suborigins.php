<?php
header("Content-Security-Policy: suborigin foo, suborigin bar");
?>
<!DOCTYPE html>
<script>
window.secret = 'I am a secret';
parent.postMessage('Done', '*');
</script>
