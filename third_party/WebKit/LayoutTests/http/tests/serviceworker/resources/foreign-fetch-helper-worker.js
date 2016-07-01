importScripts('../../resources/get-host-info.js');
var host_info = get_host_info();

self.onfetch = e => {
  var remote_url = host_info.HTTPS_REMOTE_ORIGIN +
                   '/serviceworker/resources/simple.txt?basic_sw';
  e.respondWith(fetch(remote_url));
};
