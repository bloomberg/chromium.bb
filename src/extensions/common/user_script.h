// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_USER_SCRIPT_H_
#define EXTENSIONS_COMMON_USER_SCRIPT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "extensions/common/host_id.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "url/gurl.h"

namespace base {
class Pickle;
class PickleIterator;
}

namespace extensions {

// Represents a user script, either a standalone one, or one that is part of an
// extension.
class UserScript {
 public:
  // The file extension for standalone user scripts.
  static const char kFileExtension[];

  static int GenerateUserScriptID();

  // Check if a URL should be treated as a user script and converted to an
  // extension.
  static bool IsURLUserScript(const GURL& url, const std::string& mime_type);

  // Get the valid user script schemes for the current process. If
  // canExecuteScriptEverywhere is true, this will return ALL_SCHEMES.
  static int ValidUserScriptSchemes(bool canExecuteScriptEverywhere = false);

  // TODO(rdevlin.cronin) This and RunLocation don't really belong here, since
  // they are used for more than UserScripts (e.g., tabs.executeScript()).
  // The type of injected script.
  enum InjectionType {
    // A content script specified in the extension's manifest.
    CONTENT_SCRIPT,
    // A script injected via, e.g. tabs.executeScript().
    PROGRAMMATIC_SCRIPT
  };
  // The last type of injected script; used for enum verification in IPC.
  // Update this if you add more injected script types!
  static const InjectionType INJECTION_TYPE_LAST = PROGRAMMATIC_SCRIPT;

  // Locations that user scripts can be run inside the document.
  // The three run locations must strictly follow each other in both load order
  // (i.e., start *always* comes before end) and numerically, as we use
  // arithmetic checking (e.g., curr == last + 1). So, no bitmasks here!!
  enum RunLocation {
    UNDEFINED,
    DOCUMENT_START,  // After the documentElement is created, but before
                     // anything else happens.
    DOCUMENT_END,  // After the entire document is parsed. Same as
                   // DOMContentLoaded.
    DOCUMENT_IDLE,  // Sometime after DOMContentLoaded, as soon as the document
                    // is "idle". Currently this uses the simple heuristic of:
                    // min(DOM_CONTENT_LOADED + TIMEOUT, ONLOAD), but no
                    // particular injection point is guaranteed.
    RUN_DEFERRED,  // The user script's injection was deferred for permissions
                   // reasons, and was executed at a later time.
    BROWSER_DRIVEN,  // The user script will be injected when triggered by an
                     // IPC in the browser process.
    RUN_LOCATION_LAST  // Leave this as the last item.
  };

  // Holds script file info.
  class File {
   public:
    File(const base::FilePath& extension_root,
         const base::FilePath& relative_path,
         const GURL& url);
    File();
    File(const File& other);
    ~File();

    const base::FilePath& extension_root() const { return extension_root_; }
    const base::FilePath& relative_path() const { return relative_path_; }

    const GURL& url() const { return url_; }
    void set_url(const GURL& url) { url_ = url; }

    // If external_content_ is set returns it as content otherwise it returns
    // content_
    const base::StringPiece GetContent() const {
      if (external_content_.data())
        return external_content_;
      else
        return content_;
    }
    void set_external_content(const base::StringPiece& content) {
      external_content_ = content;
    }
    void set_content(const base::StringPiece& content) {
      content_.assign(content.begin(), content.end());
    }

    // Serialization support. The content and FilePath members will not be
    // serialized!
    void Pickle(base::Pickle* pickle) const;
    void Unpickle(const base::Pickle& pickle, base::PickleIterator* iter);

   private:
    // Where the script file lives on the disk. We keep the path split so that
    // it can be localized at will.
    base::FilePath extension_root_;
    base::FilePath relative_path_;

    // The url to this script file.
    GURL url_;

    // The script content. It can be set to either loaded_content_ or
    // externally allocated string.
    base::StringPiece external_content_;

    // Set when the content is loaded by LoadContent
    std::string content_;
  };

  using FileList = std::vector<std::unique_ptr<File>>;

  // Type of a API consumer instance that user scripts will be injected on.
  enum ConsumerInstanceType { TAB, WEBVIEW };

  // Constructor. Default the run location to document end, which is like
  // Greasemonkey and probably more useful for typical scripts.
  UserScript();
  ~UserScript();

  // Performs a copy of all fields except file contents.
  static std::unique_ptr<UserScript> CopyMetadataFrom(const UserScript& other);

  const std::string& name_space() const { return name_space_; }
  void set_name_space(const std::string& name_space) {
    name_space_ = name_space;
  }

  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

  const std::string& version() const { return version_; }
  void set_version(const std::string& version) {
    version_ = version;
  }

  const std::string& description() const { return description_; }
  void set_description(const std::string& description) {
    description_ = description;
  }

  // The place in the document to run the script.
  RunLocation run_location() const { return run_location_; }
  void set_run_location(RunLocation location) { run_location_ = location; }

  // Whether to emulate greasemonkey when running this script.
  bool emulate_greasemonkey() const { return emulate_greasemonkey_; }
  void set_emulate_greasemonkey(bool val) { emulate_greasemonkey_ = val; }

  // Whether to match all frames, or only the top one.
  bool match_all_frames() const { return match_all_frames_; }
  void set_match_all_frames(bool val) { match_all_frames_ = val; }

  // Whether to match about:blank and about:srcdoc.
  bool match_about_blank() const { return match_about_blank_; }
  void set_match_about_blank(bool val) { match_about_blank_ = val; }

  // The globs, if any, that determine which pages this script runs against.
  // These are only used with "standalone" Greasemonkey-like user scripts.
  const std::vector<std::string>& globs() const { return globs_; }
  void add_glob(const std::string& glob) { globs_.push_back(glob); }
  void clear_globs() { globs_.clear(); }
  const std::vector<std::string>& exclude_globs() const {
    return exclude_globs_;
  }
  void add_exclude_glob(const std::string& glob) {
    exclude_globs_.push_back(glob);
  }
  void clear_exclude_globs() { exclude_globs_.clear(); }

  // The URLPatterns, if any, that determine which pages this script runs
  // against.
  const URLPatternSet& url_patterns() const { return url_set_; }
  void add_url_pattern(const URLPattern& pattern);
  const URLPatternSet& exclude_url_patterns() const {
    return exclude_url_set_;
  }
  void add_exclude_url_pattern(const URLPattern& pattern);

  // List of js scripts for this user script
  FileList& js_scripts() { return js_scripts_; }
  const FileList& js_scripts() const { return js_scripts_; }

  // List of css scripts for this user script
  FileList& css_scripts() { return css_scripts_; }
  const FileList& css_scripts() const { return css_scripts_; }

  const std::string& extension_id() const { return host_id_.id(); }

  const HostID& host_id() const { return host_id_; }
  void set_host_id(const HostID& host_id) { host_id_ = host_id; }

  const ConsumerInstanceType& consumer_instance_type() const {
    return consumer_instance_type_;
  }
  void set_consumer_instance_type(
      const ConsumerInstanceType& consumer_instance_type) {
    consumer_instance_type_ = consumer_instance_type;
  }

  int id() const { return user_script_id_; }
  void set_id(int id) { user_script_id_ = id; }

  // TODO(lazyboy): Incognito information is extension specific, it doesn't
  // belong here. We should be able to determine this in the renderer/ where it
  // is used.
  bool is_incognito_enabled() const { return incognito_enabled_; }
  void set_incognito_enabled(bool enabled) { incognito_enabled_ = enabled; }

  // Returns true if the script should be applied to the specified URL, false
  // otherwise.
  bool MatchesURL(const GURL& url) const;

  // Returns true if the script should be applied to the given
  // |effective_document_url| (calculated by the caller based on
  // match_about_blank()| while also taking into account whether the document's
  // frame |is_subframe| and what the |top_level_origin| is.
  bool MatchesDocument(const GURL& effective_document_url,
                       bool is_subframe) const;

  // Serializes the UserScript into a pickle. The content of the scripts and
  // paths to UserScript::Files will not be serialized!
  void Pickle(base::Pickle* pickle) const;

  // Deserializes the script from a pickle. Note that this always succeeds
  // because presumably we were the one that pickled it, and we did it
  // correctly.
  void Unpickle(const base::Pickle& pickle, base::PickleIterator* iter);

 private:
  // base::Pickle helper functions used to pickle the individual types of
  // components.
  void PickleGlobs(base::Pickle* pickle,
                   const std::vector<std::string>& globs) const;
  void PickleHostID(base::Pickle* pickle, const HostID& host_id) const;
  void PickleURLPatternSet(base::Pickle* pickle,
                           const URLPatternSet& pattern_list) const;
  void PickleScripts(base::Pickle* pickle, const FileList& scripts) const;

  // Unpickle helper functions used to unpickle individual types of components.
  void UnpickleGlobs(const base::Pickle& pickle,
                     base::PickleIterator* iter,
                     std::vector<std::string>* globs);
  void UnpickleHostID(const base::Pickle& pickle,
                      base::PickleIterator* iter,
                      HostID* host_id);
  void UnpickleURLPatternSet(const base::Pickle& pickle,
                             base::PickleIterator* iter,
                             URLPatternSet* pattern_list);
  void UnpickleScripts(const base::Pickle& pickle,
                       base::PickleIterator* iter,
                       FileList* scripts);

  // The location to run the script inside the document.
  RunLocation run_location_;

  // The namespace of the script. This is used by Greasemonkey in the same way
  // as XML namespaces. Only used when parsing Greasemonkey-style scripts.
  std::string name_space_;

  // The script's name. Only used when parsing Greasemonkey-style scripts.
  std::string name_;

  // A longer description. Only used when parsing Greasemonkey-style scripts.
  std::string description_;

  // A version number of the script. Only used when parsing Greasemonkey-style
  // scripts.
  std::string version_;

  // Greasemonkey-style globs that determine pages to inject the script into.
  // These are only used with standalone scripts.
  std::vector<std::string> globs_;
  std::vector<std::string> exclude_globs_;

  // URLPatterns that determine pages to inject the script into. These are
  // only used with scripts that are part of extensions.
  URLPatternSet url_set_;
  URLPatternSet exclude_url_set_;

  // List of js scripts defined in content_scripts
  FileList js_scripts_;

  // List of css scripts defined in content_scripts
  FileList css_scripts_;

  // The ID of the host this script is a part of. The |ID| of the
  // |host_id| can be empty if the script is a "standlone" user script.
  HostID host_id_;

  // The type of the consumer instance that the script will be injected.
  ConsumerInstanceType consumer_instance_type_;

  // The globally-unique id associated with this user script. Defaults to
  // -1 for invalid.
  int user_script_id_;

  // Whether we should try to emulate Greasemonkey's APIs when running this
  // script.
  bool emulate_greasemonkey_;

  // Whether the user script should run in all frames, or only just the top one.
  // Defaults to false.
  bool match_all_frames_;

  // Whether the user script should run in about:blank and about:srcdoc as well.
  // Defaults to false.
  bool match_about_blank_;

  // True if the script should be injected into an incognito tab.
  bool incognito_enabled_;

  DISALLOW_COPY_AND_ASSIGN(UserScript);
};

// Information we need while removing scripts from a UserScriptLoader.
struct UserScriptIDPair {
  UserScriptIDPair(int id, const HostID& host_id);
  explicit UserScriptIDPair(int id);

  int id;
  HostID host_id;
};

bool operator<(const UserScriptIDPair& a, const UserScriptIDPair& b);

using UserScriptList = std::vector<std::unique_ptr<UserScript>>;

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_USER_SCRIPT_H_
