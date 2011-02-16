// Copyright 2011 Google Inc.  All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

// This script coordinates PNaCl Client-Side translation.

// Holds all state for coordinating pnacl client-side translation.
function PnaclTranslatorState(bitcode_url, llc_id, ld_id, nexe_id, finish_cb) {
  this.bitcode_url = bitcode_url;
  this.bitcode_fd = 0;

  this.llc_plugin = llc_id;
  this.ld_plugin = ld_id;
  this.nexe_plugin = nexe_id;
  this.GetPlugin = function(plugin_id) {
    var plugin = document.getElementById(plugin_id);
    if (plugin == null) {
      // TODO: should we throw an exception?
      generalLog('GetPlugin failed to get plugin');
    }
    return plugin;
  }

  this.finish_cb = finish_cb;

  // TODO: generalize this to any other dependencies
  // besides the application bitcode file.
  // In the future we will obtain these dependencies from .nmf,
  // or from bitcode metadata, or whatever.
  // NOTE: There is a bug where large files (> 3KB?) requested through
  // urlAsNaClDesc, do not get the correct size with fstat
  // (the reason why ld_script is listed last and crt1.o is listed first).
  this.link_files = ['crt1.o',
                     'crti.o',
                     'crtbegin.o',
                     'libcrt_platform.a',
                     'crtend.o',
                     'crtn.o',
                     'libgcc_eh.a',
                     'libgcc.a',
                     'ld_script'
                     ];

  // Number of dependencies loaded.
  this.num_loaded = 0;

  // Return true if all dependencies have been loaded and FDs are available.
  this.LoadedDependencies = function() {
    // Should be |Link Files| + |Bitcode Files|
    var finished = this.num_loaded == (this.link_files.length + 1);
    if (finished) {
      generalLog('Finished Downloading Link files and Bitcode!');
    }
    return finished;
  }

  // Once a file is loaded, remember the FD.
  this.NoteLoaded = function(file_to_load, nacl_fd) {
    // Either loads a bitcode file, or a linker file.
    if (file_to_load == this.bitcode_url) {
      this.bitcode_fd = nacl_fd;
    } else {
      this.GetPlugin(this.ld_plugin).AddFile(file_to_load, nacl_fd);
    }
    this.num_loaded += 1;
  }

  // Given all the loaded FDs, look in the cache or translate, then run.
  this.TranslateAndRun = function() {
    // All the fds should be available now.
    alert('Finished Downloading Everything!');

    generalLog('Translating bitcode ' + this.bitcode_url);
    var trans_results = this.GetPlugin(this.llc_plugin).Translate(
        this.bitcode_fd);
    alert('Finished LLC');
    var obj_fd = trans_results[0];
    var obj_f_size = trans_results[1];
    generalLog('.o file size is: ' + obj_f_size);

    generalLog('Linking');
    var link_results = this.GetPlugin(this.ld_plugin).Link(obj_fd, obj_f_size);
    var nexe_fd = link_results[0];
    var nexe_f_size = link_results[1];
    alert('Finished Linking');

    generalLog('Translated! .nexe size is: ' + nexe_f_size);

    // NOTE: the size of the shm for the nexe_fd is not needed because
    // size information is in the ELF headers.
    this.GetPlugin(this.nexe_plugin).__launchExecutableFromFd(nexe_fd);
    generalLog('Nexe is now running!');
    this.finish_cb();
  }

}

function FailHandlerGenerator(ComponentName) {
  return function(object) {
    generalLog(ComponentName + ": " + object);
  }
}

// Helper to load up files needed by PNaCl.
// This has the side-effect of also initiating translation once all is done.
function PnaclFileLoader(trans_state, file_to_load) {
  this.onload = function(nacl_fd) {
    generalLog('Loaded ' + file_to_load);
    trans_state.NoteLoaded(file_to_load, nacl_fd);

    // Are we done loading? If so, translate and run!
    if (trans_state.LoadedDependencies()) {
      trans_state.TranslateAndRun();
    }
  }

  this.onfail = FailHandlerGenerator('PnaclFileLoader');
}

function StartLoadingDependencies(trans_state) {
  // Load linker dependencies
  for (var i = 0; i < trans_state.link_files.length; i++) {
    var next_file = trans_state.link_files[i];
    trans_state.GetPlugin(trans_state.ld_plugin).__urlAsNaClDesc(next_file,
                                               new PnaclFileLoader(trans_state,
                                               next_file));
  }

  // Load bitcode file
  trans_state.GetPlugin(trans_state.llc_plugin).__urlAsNaClDesc(
                                               trans_state.bitcode_url,
                                               new PnaclFileLoader(trans_state,
                                               trans_state.bitcode_url));

}

function PnaclTranslatorInit(bitcode_url, llc_id, ld_id, nexe_id, finish_cb) {
  generalLog("Entering PnaclTranslatorInit");
  var trans_state = new PnaclTranslatorState(bitcode_url,
                                             llc_id,
                                             ld_id,
                                             nexe_id,
                                             finish_cb);
  // When we have caching we could skip downloading all the dependencies
  // and translation. Then again, caching is likely not be based on this script.
  StartLoadingDependencies(trans_state);
}


//////////////////////////////////////////////////////////////////////
// Utilities

function generalLog(msg) {
  var my_log_area = document.getElementById("general_log_area");
  my_log_area.innerHTML += (msg + "<br />");
}

// TODO: add testing utils.
// E.g.,
// - set cookie status=OK for selenium, chrome ui tests, etc.
// - timing translation
// - handle timeouts
