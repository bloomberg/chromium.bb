<!DOCTYPE html>
<html>
<script>
window.onmessage = function(event) {
    var storage;
    var prefix;
    if (event.data.type == 'localStorage') {
        storage = localStorage;
        prefix = 'LOCAL_';
    } else if (event.data.type == 'sessionStorage') {
        storage = sessionStorage;
        prefix = 'SESSION_';
    } else {
        assert_unreached('Unknown storage type');
    }
    storage.setItem(prefix + "FOO2", "BAR");
    window.parent.postMessage({ "type": event.data.type, "value": storage.getItem(prefix + "_FOO1") }, '*');
};
window.parent.postMessage('ready', '*');
</script>
</html>
