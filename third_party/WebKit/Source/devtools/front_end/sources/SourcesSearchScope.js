/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * @implements {Sources.SearchScope}
 * @unrestricted
 */
Sources.SourcesSearchScope = class {
  constructor() {
    // FIXME: Add title once it is used by search controller.
    this._searchId = 0;
  }

  /**
   * @param {!Workspace.UISourceCode} uiSourceCode1
   * @param {!Workspace.UISourceCode} uiSourceCode2
   * @return {number}
   */
  static _filesComparator(uiSourceCode1, uiSourceCode2) {
    if (uiSourceCode1.isDirty() && !uiSourceCode2.isDirty())
      return -1;
    if (!uiSourceCode1.isDirty() && uiSourceCode2.isDirty())
      return 1;
    var url1 = uiSourceCode1.url();
    var url2 = uiSourceCode2.url();
    if (url1 && !url2)
      return -1;
    if (!url1 && url2)
      return 1;
    return String.naturalOrderComparator(uiSourceCode1.fullDisplayName(), uiSourceCode2.fullDisplayName());
  }

  /**
   * @override
   * @param {!Common.Progress} progress
   */
  performIndexing(progress) {
    this.stopSearch();

    var projects = this._projects();
    var compositeProgress = new Common.CompositeProgress(progress);
    for (var i = 0; i < projects.length; ++i) {
      var project = projects[i];
      var projectProgress = compositeProgress.createSubProgress(project.uiSourceCodes().length);
      project.indexContent(projectProgress);
    }
  }

  /**
   * @return {!Array.<!Workspace.Project>}
   */
  _projects() {
    var searchInAnonymousAndContentScripts = Common.moduleSetting('searchInAnonymousAndContentScripts').get();

    return Workspace.workspace.projects().filter(project => {
      if (project.type() === Workspace.projectTypes.Service)
        return false;
      if (!searchInAnonymousAndContentScripts && project.isServiceProject())
        return false;
      if (!searchInAnonymousAndContentScripts && project.type() === Workspace.projectTypes.ContentScripts)
        return false;
      return true;
    });
  }

  /**
   * @override
   * @param {!Workspace.ProjectSearchConfig} searchConfig
   * @param {!Common.Progress} progress
   * @param {function(!Sources.FileBasedSearchResult)} searchResultCallback
   * @param {function(boolean)} searchFinishedCallback
   */
  performSearch(searchConfig, progress, searchResultCallback, searchFinishedCallback) {
    this.stopSearch();
    this._searchResultCandidates = [];
    this._searchResultCallback = searchResultCallback;
    this._searchFinishedCallback = searchFinishedCallback;
    this._searchConfig = searchConfig;

    var promises = [];
    var compositeProgress = new Common.CompositeProgress(progress);
    var searchContentProgress = compositeProgress.createSubProgress();
    var findMatchingFilesProgress = new Common.CompositeProgress(compositeProgress.createSubProgress());
    for (var project of this._projects()) {
      var weight = project.uiSourceCodes().length;
      var findMatchingFilesInProjectProgress = findMatchingFilesProgress.createSubProgress(weight);
      var filesMathingFileQuery = this._projectFilesMatchingFileQuery(project, searchConfig);
      var promise =
          project
              .findFilesMatchingSearchRequest(searchConfig, filesMathingFileQuery, findMatchingFilesInProjectProgress)
              .then(this._processMatchingFilesForProject.bind(this, this._searchId, project, filesMathingFileQuery));
      promises.push(promise);
    }

    Promise.all(promises).then(this._processMatchingFiles.bind(
        this, this._searchId, searchContentProgress, this._searchFinishedCallback.bind(this, true)));
  }

  /**
   * @param {!Workspace.Project} project
   * @param {!Workspace.ProjectSearchConfig} searchConfig
   * @param {boolean=} dirtyOnly
   * @return {!Array.<string>}
   */
  _projectFilesMatchingFileQuery(project, searchConfig, dirtyOnly) {
    var result = [];
    var uiSourceCodes = project.uiSourceCodes();
    for (var i = 0; i < uiSourceCodes.length; ++i) {
      var uiSourceCode = uiSourceCodes[i];
      if (!uiSourceCode.contentType().isTextType())
        continue;
      var binding = Persistence.persistence.binding(uiSourceCode);
      if (binding && binding.network === uiSourceCode)
        continue;
      if (dirtyOnly && !uiSourceCode.isDirty())
        continue;
      if (this._searchConfig.filePathMatchesFileQuery(uiSourceCode.fullDisplayName()))
        result.push(uiSourceCode.url());
    }
    result.sort(String.naturalOrderComparator);
    return result;
  }

  /**
   * @param {number} searchId
   * @param {!Workspace.Project} project
   * @param {!Array<string>} filesMathingFileQuery
   * @param {!Array<string>} files
   */
  _processMatchingFilesForProject(searchId, project, filesMathingFileQuery, files) {
    if (searchId !== this._searchId) {
      this._searchFinishedCallback(false);
      return;
    }

    files.sort(String.naturalOrderComparator);
    files = files.intersectOrdered(filesMathingFileQuery, String.naturalOrderComparator);
    var dirtyFiles = this._projectFilesMatchingFileQuery(project, this._searchConfig, true);
    files = files.mergeOrdered(dirtyFiles, String.naturalOrderComparator);

    var uiSourceCodes = [];
    for (var file of files) {
      var uiSourceCode = project.uiSourceCodeForURL(file);
      if (!uiSourceCode)
        continue;
      var script = Bindings.DefaultScriptMapping.scriptForUISourceCode(uiSourceCode);
      if (script && !script.isAnonymousScript())
        continue;
      uiSourceCodes.push(uiSourceCode);
    }
    uiSourceCodes.sort(Sources.SourcesSearchScope._filesComparator);
    this._searchResultCandidates =
        this._searchResultCandidates.mergeOrdered(uiSourceCodes, Sources.SourcesSearchScope._filesComparator);
  }

  /**
   * @param {number} searchId
   * @param {!Common.Progress} progress
   * @param {function()} callback
   */
  _processMatchingFiles(searchId, progress, callback) {
    if (searchId !== this._searchId) {
      this._searchFinishedCallback(false);
      return;
    }

    var files = this._searchResultCandidates;
    if (!files.length) {
      progress.done();
      callback();
      return;
    }

    progress.setTotalWork(files.length);

    var fileIndex = 0;
    var maxFileContentRequests = 20;
    var callbacksLeft = 0;

    for (var i = 0; i < maxFileContentRequests && i < files.length; ++i)
      scheduleSearchInNextFileOrFinish.call(this);

    /**
     * @param {!Workspace.UISourceCode} uiSourceCode
     * @this {Sources.SourcesSearchScope}
     */
    function searchInNextFile(uiSourceCode) {
      if (uiSourceCode.isDirty())
        contentLoaded.call(this, uiSourceCode, uiSourceCode.workingCopy());
      else
        uiSourceCode.requestContent().then(contentLoaded.bind(this, uiSourceCode));
    }

    /**
     * @this {Sources.SourcesSearchScope}
     */
    function scheduleSearchInNextFileOrFinish() {
      if (fileIndex >= files.length) {
        if (!callbacksLeft) {
          progress.done();
          callback();
          return;
        }
        return;
      }

      ++callbacksLeft;
      var uiSourceCode = files[fileIndex++];
      setTimeout(searchInNextFile.bind(this, uiSourceCode), 0);
    }

    /**
     * @param {!Workspace.UISourceCode} uiSourceCode
     * @param {?string} content
     * @this {Sources.SourcesSearchScope}
     */
    function contentLoaded(uiSourceCode, content) {
      /**
       * @param {!Common.ContentProvider.SearchMatch} a
       * @param {!Common.ContentProvider.SearchMatch} b
       */
      function matchesComparator(a, b) {
        return a.lineNumber - b.lineNumber;
      }

      progress.worked(1);
      var matches = [];
      var queries = this._searchConfig.queries();
      if (content !== null) {
        for (var i = 0; i < queries.length; ++i) {
          var nextMatches = Common.ContentProvider.performSearchInContent(
              content, queries[i], !this._searchConfig.ignoreCase(), this._searchConfig.isRegex());
          matches = matches.mergeOrdered(nextMatches, matchesComparator);
        }
      }
      if (matches) {
        var searchResult = new Sources.FileBasedSearchResult(uiSourceCode, matches);
        this._searchResultCallback(searchResult);
      }

      --callbacksLeft;
      scheduleSearchInNextFileOrFinish.call(this);
    }
  }

  /**
   * @override
   */
  stopSearch() {
    ++this._searchId;
  }
};
