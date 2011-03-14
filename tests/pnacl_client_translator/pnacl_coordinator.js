// Copyright 2011 Google Inc.  All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

// This script coordinates PNaCl Client-Side translation.
// NOTE: This is currently using deprecated facilities like __urlAsNaClDesc.


function $(id) {
  return document.getElementById(id);
}

/// Utilities for setting up a webpage with PNaCl components. These will be
/// obsoleted by nmf changes that make embedding a PNaCl component more natural.

// Class holding the utility embeds that we care about for PNaCl.
function PnaclEmbeds() {
  this.embedsToWaitFor_ = [];
  this.llcEmbed_ = null;
  this.ldEmbed_ = null;

  this.addEmbedsToWaitFor = function(embed) {
    this.embedsToWaitFor_.push(embed);
  }

  this.getEmbedsToWaitFor = function() {
    return this.embedsToWaitFor_;
  }

  this.setLLC = function(embed) {
    this.llcEmbed_ = embed;
    this.addEmbedsToWaitFor(embed);
  }

  this.getLLC = function(embed) {
    return this.llcEmbed_;
  }

  this.setLD = function(embed) {
    this.ldEmbed_ = embed;
    this.addEmbedsToWaitFor(embed);
  }

  this.getLD = function() {
    return this.ldEmbed_;
  }
}

// @public
// Creates the embed tags for the PNaCl translator nexes and
// returns the collection of wrapped in a PnaclEmbed object.
function injectUtilityEmbeds() {
  // Create a spot to inject the pnacl embed tags.
  var pnaclDiv = document.createElement('div');
  document.body.appendChild(pnaclDiv);
  var pnaclEmbeds = new PnaclEmbeds();

  pnaclEmbeds.setLLC(injectAnEmbed(pnaclDiv, 'llc', 'llc_plugin'));
  pnaclEmbeds.setLD(injectAnEmbed(pnaclDiv, 'ld', 'ld_plugin'));

  return pnaclEmbeds;
}

function injectAnEmbed(pnaclDiv, nacl_url, id) {
  var embed = document.createElement('embed');

  embed.width = 0;
  embed.height = 0;
  embed.type = 'application/x-nacl';
  embed.id = id; // mostly for debugging...
  // TODO(jvoung): Use nmf since src is going to be deprecated.
  embed.src = nacl_url;

  pnaclDiv.appendChild(embed);
  return embed;
}

// Holds all state for coordinating pnacl client-side translation.
function PnaclTranslatorState(bitcodeURL,
                              pnaclEmbeds,
                              nexeEmbed,
                              finishCallback) {
  this.bitcodeURL_ = bitcodeURL;
  this.bitcodeFD_ = 0;
  this.kNumBCFiles = 1;
  this.pnaclPlugins_ = pnaclEmbeds;
  this.nexePlugin_ = nexeEmbed;
  this.finishCallback_ = finishCallback;

  // These must match up with the ISA strings in the NMF.
  this.X8632 = 'x86-32';
  this.X8664 = 'x86-64';
  this.ARM = 'arm';
  this.getArch = function() {
    var sandboxISA = this.pnaclPlugins_.getLLC().__getSandboxISA();
    if (sandboxISA) {
      if (sandboxISA == this.X8632 || sandboxISA == this.X8664 ||
          sandboxISA == this.ARM) {
        return sandboxISA;
      } else {
        throw 'Unknown arch!';
      }
    } else {
      throw 'Call to __getSandboxISA failed!';
    }
  }

  /// BEGIN
  /// Linker stuff
  ///

  // TODO(jvoung): In the future we will obtain these dependencies from .nmf:
// http://code.google.com/p/nativeclient/issues/detail?id=885
// http://code.google.com/p/nativeclient/wiki/NameResolutionForDynamicLibraries

  // This is a URL -> LD internal filename map.
  // For now, assume files are installed alongside the bitcode.
  // Scons / the browser test framework will install the link files for each
  // architecture under the same name. Realistically, a webserver
  // will have different URLs for each, but realistically we would
  // be specifying these from a manifest file.
  this.linkFiles_ = {
    'ld_script' : 'ld_script',
    'crt1.o' : 'crt1.o',
    'crti.o' : 'crti.o',
    'crtbegin.o' : 'crtbegin.o',
    'libcrt_platform.a' : 'libcrt_platform.a',
    'libgcc_eh.a' : 'libgcc_eh.a',
    'libgcc.a' : 'libgcc.a',
    'crtend.o' : 'crtend.o',
    'crtn.o' : 'crtn.o'
  };

  this.getLinkFiles = function() {
    return this.linkFiles_;
  }

  this.getLinkFile = function(file_url) {
    return this.getLinkFiles()[file_url];
  }

  this.getNumLinkFiles = function() {
    var i = 0;
    for (var l in this.getLinkFiles()) {
      i++;
    }
    return i;
  }

  ///
  /// Linker stuff
  /// END

  // Number of dependencies loaded.
  this.numLoaded_ = 0;

  // Return true if all dependencies have been loaded and FDs are available.
  this.didLoadDependencies = function() {
    // Should be |Link Files| + |Bitcode Files|
    var finished = this.numLoaded_ == (this.getNumLinkFiles() +
                                       this.kNumBCFiles);
    if (finished) {
      this.generalLog('Finished Downloading Link files and Bitcode!');
    }
    return finished;
  }

  // Once a file is loaded, remember the FD.
  this.noteLoaded = function(fileToLoad, nacl_fd) {
    // Either loads a bitcode file, or a linker file.
    if (fileToLoad == this.bitcodeURL_) {
      this.bitcodeFD_ = nacl_fd;
    } else {
      // We need to map the URL to the filename that LD will use to
      // refer to the file internally.
      var filename = this.getLinkFile(fileToLoad);
      this.pnaclPlugins_.getLD().AddFile(filename, nacl_fd);
    }
    this.numLoaded_ += 1;
  }

  /// BEGIN
  /// LLC stuff
  ///

  this.getLLCArgs = function() {
    return this.llcArgs_[this.getArch()];
  }

  this.llcArgs64_ = ['-march=x86-64',
                     '-mcpu=core2',
                     '-asm-verbose=false',
                     '-filetype=obj'
                     ];
  this.llcArgs32_ = ['-march=x86',
                     '-mcpu=pentium4',
                     '-asm-verbose=false',
                     '-filetype=obj'
                     ];
  // TODO(jvoung): slap in ARM versions of command line flags.
  this.llcArgs_ = {};
  this.llcArgs_[this.X8632] = this.llcArgs32_;
  this.llcArgs_[this.X8664] = this.llcArgs64_;

  this.setLLCArgs = function() {
    var llcArgs = this.getLLCArgs();
    for (var i = 0; i < llcArgs.length; i++) {
      this.pnaclPlugins_.getLLC().AddArg(llcArgs[i]);
    }
  }

  ///
  /// LLC stuff
  /// END


  /// BEGIN
  /// Main coordination stuff
  ///

  // Given all the loaded FDs, look in the cache or translate, then run.
  this.translateAndRun = function() {
    this.generalLog('Translating bitcode ' + this.bitcodeURL_);
    this.timeStart('Compile');
    this.setLLCArgs();
    var transResults = this.pnaclPlugins_.getLLC().Translate(this.bitcodeFD_);
    if (!transResults) {
      throw 'Call to LLC.Translate failed';
    }
    this.timeEnd('Compile');
    var objFD = transResults[0];
    var objSize = transResults[1];
    this.generalLog('.o file size is: ' + objSize);

    this.generalLog('Linking');
    this.timeStart('Link');
    var linkResults = this.pnaclPlugins_.getLD().Link(objFD, objSize);
    if (!linkResults) {
      throw 'Call to LD.Link failed';
    }
    var nexeFD = linkResults[0];
    var nexeSize = linkResults[1];
    this.timeEnd('Link');

    this.generalLog('Translated! .nexe size is: ' + nexeSize);

    // NOTE: the size of the shm 'file' for the nexeFD is not needed because
    // size information is already in the ELF headers.
    this.nexePlugin_.__launchExecutableFromFd(nexeFD);
    this.generalLog('Nexe is now running!');
    this.finishCallback_();
  }

  this.startLoadingDependencies = function() {
    this.timeStart('Downloads');

    // Load linker dependencies
    for (var fileURL in this.getLinkFiles()) {
      this.pnaclPlugins_.getLD().__urlAsNaClDesc(fileURL,
                                        new PnaclFileLoader(this, fileURL));
    }

    // Load bitcode file
    this.pnaclPlugins_.getLLC().__urlAsNaClDesc(
        this.bitcodeURL_,
        new PnaclFileLoader(this, this.bitcodeURL_));
  }

  ///
  /// Main coordination stuff
  /// END

  /// BEGIN
  /// Timing Utilities
  ///

  // To be useful, we need this emitted to the buildbot stdout in the form
  // "RESULT...". It may be best to do this in the plugin.
  // Perhaps this should be done by an external library like nacltest.js
  this.timeStartData = {};

  this.timeStart = function(key) {
    this.timeStartData[key] = (new Date).getTime();
  }

  this.timeEnd = function(key) {
    var end_time = (new Date).getTime();
    this.generalLog(key + ' took: ' +
                    (end_time - this.timeStartData[key]) + 'ms');
  }

  ///
  /// Timing Utilities
  /// END

  this.myLogger = new LogToPage('#C0FFEE');
  this.generalLog = function(msg) {
    this.myLogger.logToPage(msg);
  }

  this.failHandlerGenerator = function(ComponentName) {
    return function(object) {
      this.generalLog(ComponentName + ' FAILED: ' + object);
      throw object;
    }
  }

}


// @public
function pnaclTranslatorInit(bitcodeURL,
                             pnaclEmbeds,
                             nexeModule,
                             finishCallback) {
  var translatorState = new PnaclTranslatorState(bitcodeURL,
                                                 pnaclEmbeds,
                                                 nexeModule,
                                                 finishCallback);
  translatorState.generalLog('Entering pnaclTranslatorInit');
  // When we have caching we could skip downloading all the dependencies
  // and translation. Then again, caching is likely not be based on this script.
  translatorState.startLoadingDependencies(translatorState);
}


function LogToPage(borderColor) {
  this.myLogArea_ = null;
  this.borderColor_ = borderColor;

  this.makeMyLogArea_ = function() {
    this.myLogArea_ = document.createElement('div');
    this.myLogArea_.style.border = '2px solid ' + borderColor;
    this.myLogArea_.style.padding = '10px';
    var headerNode = document.createTextNode('PNACL LOGS: ');
    var br = document.createElement('br');
    this.myLogArea_.appendChild(headerNode);
    this.myLogArea_.appendChild(br);
    document.body.appendChild(this.myLogArea_);
  }

  this.logToPage = function(msg) {
    if (!this.myLogArea_) {
      this.makeMyLogArea_();
    }
    var div = document.createElement('div');
    // Preserve whitespace formatting.
    div.style['white-space'] = 'pre';
    var mNode = document.createTextNode(msg);
    div.appendChild(mNode);
    this.myLogArea_.appendChild(div);
  }
}

//////////////////////////////////////////////////////////////////////
// Other Utilities

// Helper to load up files needed by PNaCl.
// This has the side-effect of also initiating translation once all is done.
function PnaclFileLoader(translatorState, fileToLoad) {
  this.onload = function(nacl_fd) {
    translatorState.generalLog('Loaded ' + fileToLoad);
    translatorState.noteLoaded(fileToLoad, nacl_fd);

    // Are we done loading? If so, translate and run!
    if (translatorState.didLoadDependencies()) {
      translatorState.timeEnd('Downloads');
      translatorState.translateAndRun();
    }
  }

  this.onfail = translatorState.failHandlerGenerator('PnaclFileLoader');
}
