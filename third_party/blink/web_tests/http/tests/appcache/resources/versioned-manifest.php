<?php
require_once '../../resources/portabilityLayer.php';

$tmpFile = sys_get_temp_dir() . "/" . "appcache_manifest_counter";

function getCount($file)
{
    if (!file_exists($file)) {
        file_put_contents($file, "0");
        return "0";
    }
    return file_get_contents($file);
}

function stepCounter($file)
{
    if (file_exists($file)) {
        $value = getCount($file);
        file_put_contents($file, ++$value);
    }
}

header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
header("Cache-Control: no-cache, must-revalidate");
header("Pragma: no-cache");
header("Content-Type: text/cache-manifest");

if ("step" == $_GET['command'])
    stepCounter($tmpFile);

print("CACHE MANIFEST\n");
print("# version " . getCount($tmpFile) . "\n");
print("counter.php\n");
print("uncacheable-resource.php\n"); // with Cache-control: no-store
print("NETWORK:\n");
print("versioned-manifest.php?command=\n");
?>
