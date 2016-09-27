<?php
if ($_GET["suborigin"]) {
    header("Suborigin: " . $_GET["suborigin"]);
}
?>
<!DOCTYPE html>
<html>
<script>
var id = '<?php echo($_GET["id"]); ?>';
var resource_url = '<?php echo($_GET["resource"]); ?>'

var is_mem_cached = window.internals.isLoadingFromMemoryCache(resource_url);

fetch(resource_url, {mode: 'no-cors'})
  .then(function(response) {
      window.parent.postMessage({
          'type': 'response',
          'is_mem_cached': is_mem_cached,
          'id': id
        }, '*');
    })
  .catch(function(e) {
      window.parent.postMessage({
          'type': 'error',
          'is_mem_cached': is_mem_cached,
          'id': id
        }, '*');
    });
</script>
</html>
