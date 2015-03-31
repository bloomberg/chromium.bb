;;; gn.el - A major mode for editing gn files.

;; Copyright 2015 The Chromium Authors. All rights reserved.
;; Use of this source code is governed by a BSD-style license that can be
;; found in the LICENSE file.

;; Put this somewhere in your load-path and
;; (require 'gn)

;; TODO(erg): There's a lot of general improvements that could be made here:
;;
;; - We syntax highlight builtin actions, but don't highlight instantiations of
;;   templates. Should we?
;; - `fill-paragraph' works for comments, but when pointed at code, breaks
;;   spectacularly.
;; - Might want to support `imenu' users. Even if it's just a list of toplevel
;;   targets?

(eval-when-compile (require 'cl))       ;For the `case' macro.
(require 'smie)

(defvar gn-font-lock-target-declaration-keywords
  '("action" "action_foreach" "copy" "executable" "group"
    "shared_library" "source_set" "static_library"))

(defvar gn-font-lock-buildfile-fun-keywords
  '("assert" "config" "declare_args" "defined" "exec_script" "foreach"
    "get_label_info" "get_path_info" "get_target_outputs" "getenv" "import"
    "print" "process_file_template" "read_file" "rebase_path"
    "set_default_toolchain" "set_defaults" "set_sources_assignment_filter"
    "template" "tool" "toolchain" "toolchain_args" "write_file"))

(defvar gn-font-lock-predefined-var-keywords
  '("current_cpu" "current_os" "current_toolchain" "default_toolchain"
    "host_cpu" "host_os" "python_path" "root_build_dir" "root_gen_dir"
    "root_out_dir" "target_cpu" "target_gen_dir" "target_os" "target_out_dir"))

(defvar gn-font-lock-var-keywords
  '("all_dependent_configs" "allow_circular_includes_from" "args" "cflags"
    "cflags_c" "cflags_cc" "cflags_objc" "cflags_objcc" "check_includes"
    "complete_static_lib" "configs" "data" "data_deps" "defines" "depfile"
    "deps" "forward_dependent_configs_from" "include_dirs" "inputs"
    "ldflags" "lib_dirs" "libs" "output_extension" "output_name" "outputs"
    "public" "public_configs" "public_deps" "script" "sources" "testonly"
    "visibility"))

(defconst gn-font-lock-keywords
  `((,(regexp-opt gn-font-lock-target-declaration-keywords 'words) .
     font-lock-keyword-face)
    (,(regexp-opt gn-font-lock-buildfile-fun-keywords 'words) .
     font-lock-function-name-face)
    (,(regexp-opt gn-font-lock-predefined-var-keywords 'words) .
     font-lock-constant-face)
    (,(regexp-opt gn-font-lock-var-keywords 'words) .
     font-lock-variable-name-face)))

(defvar gn-indent-basic 2)

(defun gn-smie-rules (kind token)
  "These are slightly modified indentation rules from the SMIE
  Indentation Example info page. This changes the :before rule
  and adds a :list-intro to handle our x = [ ] syntax."
  (pcase (cons kind token)
    (`(:elem . basic) gn-indent-basic)
    (`(,_ . ",") (smie-rule-separator kind))
    (`(:list-intro . "") gn-indent-basic)
    (`(:before . ,(or `"[" `"(" `"{"))
     (if (smie-rule-hanging-p) (smie-rule-parent)))
    (`(:before . "if")
     (and (not (smie-rule-bolp)) (smie-rule-prev-p "else")
          (smie-rule-parent)))))

;;;###autoload
(define-derived-mode gn-mode prog-mode "GN"
  "Major mode for editing gn (Generate Ninja)."

  (setq-local font-lock-defaults '(gn-font-lock-keywords))

  (setq-local comment-use-syntax t)
  (setq-local comment-start "#")
  (setq-local comment-end "")

  (smie-setup nil #'gn-smie-rules)
  (setq-local smie-indent-basic gn-indent-basic)

  ;; python style comment: “# …”
  (modify-syntax-entry ?# "< b" gn-mode-syntax-table)
  (modify-syntax-entry ?\n "> b" gn-mode-syntax-table)
  (modify-syntax-entry ?_ "w" gn-mode-syntax-table))

;;;###autoload
(add-to-list 'auto-mode-alist '("^BUILD.gn$" . gn-mode))
(add-to-list 'auto-mode-alist '("\\.gni$'" . gn-mode))

(provide 'gn-mode)
