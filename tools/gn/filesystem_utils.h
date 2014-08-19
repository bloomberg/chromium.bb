// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_FILESYSTEM_UTILS_H_
#define TOOLS_GN_FILESYSTEM_UTILS_H_

#include <string>

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "tools/gn/settings.h"
#include "tools/gn/target.h"

class Err;
class Location;
class Value;

// Returns the extension, not including the dot, for the given file type on the
// given system.
//
// Some targets make multiple files (like a .dll and an import library). This
// function returns the name of the file other targets should depend on and
// link to (so in this example, the import library).
const char* GetExtensionForOutputType(Target::OutputType type,
                                      Settings::TargetOS os);

std::string FilePathToUTF8(const base::FilePath::StringType& str);
inline std::string FilePathToUTF8(const base::FilePath& path) {
  return FilePathToUTF8(path.value());
}
base::FilePath UTF8ToFilePath(const base::StringPiece& sp);

// Extensions -----------------------------------------------------------------

// Returns the index of the extension (character after the last dot not after a
// slash). Returns std::string::npos if not found. Returns path.size() if the
// file ends with a dot.
size_t FindExtensionOffset(const std::string& path);

// Returns a string piece pointing into the input string identifying the
// extension. Note that the input pointer must outlive the output.
base::StringPiece FindExtension(const std::string* path);

// Filename parts -------------------------------------------------------------

// Returns the offset of the character following the last slash, or
// 0 if no slash was found. Returns path.size() if the path ends with a slash.
// Note that the input pointer must outlive the output.
size_t FindFilenameOffset(const std::string& path);

// Returns a string piece pointing into the input string identifying the
// file name (following the last slash, including the extension). Note that the
// input pointer must outlive the output.
base::StringPiece FindFilename(const std::string* path);

// Like FindFilename but does not include the extension.
base::StringPiece FindFilenameNoExtension(const std::string* path);

// Removes everything after the last slash. The last slash, if any, will be
// preserved.
void RemoveFilename(std::string* path);

// Returns if the given character is a slash. This allows both slashes and
// backslashes for consistency between Posix and Windows (as opposed to
// FilePath::IsSeparator which is based on the current platform).
inline bool IsSlash(const char ch) {
  return ch == '/' || ch == '\\';
}

// Returns true if the given path ends with a slash.
bool EndsWithSlash(const std::string& s);

// Path parts -----------------------------------------------------------------

// Returns a string piece pointing into the input string identifying the
// directory name of the given path, including the last slash. Note that the
// input pointer must outlive the output.
base::StringPiece FindDir(const std::string* path);

// Returns the substring identifying the last component of the dir, or the
// empty substring if none. For example "//foo/bar/" -> "bar".
base::StringPiece FindLastDirComponent(const SourceDir& dir);

// Verifies that the given string references a file inside of the given
// directory. This is pretty stupid and doesn't handle "." and "..", etc.,
// it is designed for a sanity check to keep people from writing output files
// to the source directory accidentally.
//
// The origin will be blamed in the error.
//
// If the file isn't in the dir, returns false and sets the error. Otherwise
// returns true and leaves the error untouched.
bool EnsureStringIsInOutputDir(const SourceDir& dir,
                               const std::string& str,
                               const ParseNode* origin,
                               Err* err);

// ----------------------------------------------------------------------------

// Returns true if the input string is absolute. Double-slashes at the
// beginning are treated as source-relative paths. On Windows, this handles
// paths of both the native format: "C:/foo" and ours "/C:/foo"
bool IsPathAbsolute(const base::StringPiece& path);

// Given an absolute path, checks to see if is it is inside the source root.
// If it is, fills a source-absolute path into the given output and returns
// true. If it isn't, clears the dest and returns false.
//
// The source_root should be a base::FilePath converted to UTF-8. On Windows,
// it should begin with a "C:/" rather than being our SourceFile's style
// ("/C:/"). The source root can end with a slash or not.
//
// Note that this does not attempt to normalize slashes in the output.
bool MakeAbsolutePathRelativeIfPossible(const base::StringPiece& source_root,
                                        const base::StringPiece& path,
                                        std::string* dest);

// Converts a directory to its inverse (e.g. "/foo/bar/" -> "../../").
// This will be the empty string for the root directories ("/" and "//"), and
// in all other cases, this is guaranteed to end in a slash.
std::string InvertDir(const SourceDir& dir);

// Collapses "." and sequential "/"s and evaluates "..".
void NormalizePath(std::string* path);

// Converts slashes to backslashes for Windows. Keeps the string unchanged
// for other systems.
void ConvertPathToSystem(std::string* path);

// Takes a source-absolute path (must begin with "//") and makes it relative
// to the given directory, which also must be source-absolute.
std::string RebaseSourceAbsolutePath(const std::string& input,
                                     const SourceDir& dest_dir);

// Returns the given directory with no terminating slash at the end, such that
// appending a slash and more stuff will produce a valid path.
//
// If the directory refers to either the source or system root, we'll append
// a "." so this remains valid.
std::string DirectoryWithNoLastSlash(const SourceDir& dir);

// Returns the "best" SourceDir representing the given path. If it's inside the
// given source_root, a source-relative directory will be returned (e.g.
// "//foo/bar.cc". If it's outside of the source root, a system-absolute
// directory will be returned.
SourceDir SourceDirForPath(const base::FilePath& source_root,
                           const base::FilePath& path);

// Like SourceDirForPath but returns the SourceDir representing the current
// directory.
SourceDir SourceDirForCurrentDirectory(const base::FilePath& source_root);

// Given the label of a toolchain and whether that toolchain is the default
// toolchain, returns the name of the subdirectory for that toolchain's
// output. This will be the empty string to indicate that the toolchain outputs
// go in the root build directory. Otherwise, the result will end in a slash.
std::string GetOutputSubdirName(const Label& toolchain_label, bool is_default);

// -----------------------------------------------------------------------------

// These functions return the various flavors of output and gen directories.
SourceDir GetToolchainOutputDir(const Settings* settings);
SourceDir GetToolchainOutputDir(const BuildSettings* build_settings,
                                const Label& label, bool is_default);

SourceDir GetToolchainGenDir(const Settings* settings);
OutputFile GetToolchainGenDirAsOutputFile(const Settings* settings);
SourceDir GetToolchainGenDir(const BuildSettings* build_settings,
                             const Label& toolchain_label,
                             bool is_default);

SourceDir GetOutputDirForSourceDir(const Settings* settings,
                                   const SourceDir& source_dir);
OutputFile GetOutputDirForSourceDirAsOutputFile(const Settings* settings,
                                                const SourceDir& source_dir);

SourceDir GetGenDirForSourceDir(const Settings* settings,
                                 const SourceDir& source_dir);
OutputFile GetGenDirForSourceDirAsOutputFile(const Settings* settings,
                                             const SourceDir& source_dir);

SourceDir GetTargetOutputDir(const Target* target);
OutputFile GetTargetOutputDirAsOutputFile(const Target* target);
SourceDir GetTargetGenDir(const Target* target);
OutputFile GetTargetGenDirAsOutputFile(const Target* target);

SourceDir GetCurrentOutputDir(const Scope* scope);
SourceDir GetCurrentGenDir(const Scope* scope);

#endif  // TOOLS_GN_FILESYSTEM_UTILS_H_
