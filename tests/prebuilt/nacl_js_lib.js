// JavaScript Library for Nacl Tests and Demos

function NaclLib(embed_name, status_id, num_retries) {
   this.embed_name_ = embed_name;
   this.statusfield_ = document.getElementById(status_id);
   this.status_ = "WAIT";
   this.message_ = "";
   this.handler_ = null;
   this.retries_ = num_retries;
};


NaclLib.prototype.getStatus = function() {
  return this.status_;
};


NaclLib.prototype.getMessage = function() {
  return this.message_;
};


NaclLib.prototype.cleanUp = function() {
  if (this.handler_) {
     clearInterval(this._handler);
     this.handler_ = null;
  }
};


NaclLib.prototype.setStatus = function() {
  this.statusfield_.innerHTML =
      this.status_ + ": " + this.message_;
};


NaclLib.prototype.setStatusWait = function(message) {
  this.status_ = "WAIT";
  this.message_ = "" + this.retries_ + " " + message;
  this.setStatus()
};


NaclLib.prototype.setStatusError = function(message) {
  this.status_ = "ERROR";
  this.message_ = message;
  this.setStatus()
};


NaclLib.prototype.setStatusSuccess = function(message) {
  this.status_ = "SUCCESS";
  this.message_ = message;
  this.setStatus()
};


NaclLib.prototype.numModulesReady = function(modules) {
  var count = 0;
  for (var i = 0; i < modules.length; i++) {
    if (modules[i].__moduleReady == 1) {
      count += 1;
    }
  }
  return count;
};


NaclLib.prototype.areTherePluginProblems = function(modules) {
  for (var i = 0; i < modules.length; i++) {
    if (modules[i].__moduleReady == undefined) return 1;
  }
  return 0;
};


NaclLib.prototype.checkModuleReadiness = function() {
  // Work around bug that does not disable the handler.
  if (!this.handler_)
    return;

  if (this.retries_ == 0) {
    this.cleanUp();
    this.setStatusError("The Native Client modules are loading too slowly");
    return;
  }
  this.retries_ -= 1;

  // Find all elements with name "this.embed_name_". This should be the list
  // of all NaCl modules on the page. Note that passing in such a list at
  // initialization time would sometimes pass the list of scriptable objects
  // (as desired) but would sometimes pass the list of embed tags, depending
  // on a start-up race condition. As such, pass the "name" attribute of the
  // <embed> tags and then gather the list of all of those scriptable objects
  // each time this method is invoked.
  var module_list = document.getElementsByName(this.embed_name_);
  var num_ready = this.numModulesReady(module_list);

  if (module_list.length == num_ready) {
    if (this.wait) {
      var result = this.wait();
      if (result) {
        this.setStatusWait(result);
        return;
      }
    }

    this.cleanUp();

    var result;
    try {
      result = this.test();
    } catch(e) {
      this.setStatusError(e);
      return;
    }

    if (result == "") {
      this.setStatusSuccess("");
    } else {
      this.setStatusError(result);
    }

    return;
  }

  this.setStatusWait("Loaded " + num_ready + "/" + module_list.length +
                     " modules");

  if (this.areTherePluginProblems(module_list)) {
    this.cleanUp();
    this.setStatusError("The Native Client plugin was unable to load");
    return;
  }
};


// Workaround for JS inconsistent scoping behavior
function wrapperForCheckModuleReadiness(that) {
  that.checkModuleReadiness();
}


NaclLib.prototype.waitForModulesAndRunTests = function() {
  // avoid regsitering two handlers
  if (!this.handler_) {
    this.handler_ = setInterval(wrapperForCheckModuleReadiness, 100, this);
  }
};
