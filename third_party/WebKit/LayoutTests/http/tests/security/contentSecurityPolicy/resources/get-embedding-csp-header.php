<?php
    $embedding_csp_header = isset($_SERVER['HTTP_EMBEDDING_CSP']) ? addslashes($_SERVER['HTTP_EMBEDDING_CSP']) : null;
?>

<script>
  var response = {};
  response['src'] = '../resources/get-embedding-csp-header.php';
  response['embedding_csp'] = <?php echo is_null($embedding_csp_header) ? "null" : "\"${embedding_csp_header}\""; ?>;
  window.top.postMessage(response, '*');
</script> 