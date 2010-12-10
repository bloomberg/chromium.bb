; To use this,
; 1) Add to init.el:
;    (setq-default chrome-root "/path/to/chrome/src/")
;    (add-to-list 'load-path (concat chrome-root "tools/emacs"))
;    (require 'trybot)
; 2) Run on trybot output:
;    M-x trybot
;
; To hack on this,
; M-x eval-buffer
; M-x trybot-test-win  or  M-x trybot-test-mac

(defvar chrome-root nil
  "Path to the src/ directory of your Chrome checkout.")

(defun get-chrome-root ()
  (or chrome-root default-directory))

; Hunt down from the top, case correcting each path component as needed.
; Currently does not keep a cache.  Returns nil if no matching file can be
; figured out.
(defun case-corrected-filename (filename)
  (save-match-data
    (let ((path-components (split-string filename "/"))
          (corrected-path (file-name-as-directory (get-chrome-root))))
      (mapc
       (function
        (lambda (elt)
          (if corrected-path
              (let ((next-component
                     (car (member-ignore-case
                           elt (directory-files corrected-path)))))
                (setq corrected-path
                      (and next-component
                           (file-name-as-directory
                            (concat corrected-path next-component))))))))
       path-components)
      (if corrected-path
          (file-relative-name (directory-file-name corrected-path)
                              (get-chrome-root))
        nil))))

(defun trybot-fixup ()
  "Parse and fixup the contents of the current buffer as trybot output."

  ;; Fixup paths.
  (cd (get-chrome-root))

  ;; Fix up path references.
  ; XXX is there something I should so so this stuff doesn't end up on the
  ; undo stack?
  (goto-char (point-min))
  ; Fix Windows paths ("d:\...\src\").
  (while (re-search-forward "\\(^.:\\\\.*\\\\src\\\\\\)\\(.*?\\)[(:]" nil t)
    (replace-match "" nil t nil 1)
    ; Line now looks like:
    ;  foo\bar\baz.cc error message here
    ; We want to fixup backslashes in path into forward slashes, without
    ; modifying the error message - by matching up to the first colon above
    ; (which will be just beyond the end of the filename) we can use the end of
    ; the match as a limit.
    (subst-char-in-region (point) (match-end 0) ?\\ ?/)
    ; See if we can correct the file name casing.
    (let ((filename (buffer-substring (match-beginning 2) (match-end 2))))
      (if (and (not (file-exists-p filename))
               (setq filename (case-corrected-filename filename)))
          (replace-match filename t t nil 2))))

  ; Fix Linux/Mac paths ("/b/build/.../src/").
  (goto-char (point-min))
  (while (re-search-forward "^/b/build/[^ ]*/src/" nil t)
    (replace-match ""))

  ;; Clean up and switch into compilation mode.
  (goto-char (point-min))
  (compilation-mode))

(defun trybot-test (filename)
  "Load the given test data filename and do the trybot parse on it."

  (switch-to-buffer (get-buffer-create "*trybot-test*"))
  (let ((inhibit-read-only t)
        (coding-system-for-read 'utf-8-dos))
    (erase-buffer)
    (insert-file-contents
       (concat (get-chrome-root) "tools/emacs/" filename))

    (trybot-fixup)))

(defun trybot-test-win ()
  "Load the Windows test data and do the trybot parse on it."
  (interactive)
  (trybot-test "trybot-windows.txt"))
(defun trybot-test-mac ()
  "Load the Mac test data and do the trybot parse on it."
  (interactive)
  (trybot-test "trybot-mac.txt"))
(defun trybot-test-linux ()
  "Load the Linux test data and do the trybot parse on it."
  (interactive)
  (trybot-test "trybot-linux.txt"))

(defun trybot (url)
  "Fetch a trybot URL and fix up the output into a compilation-mode buffer."
  (interactive "sURL to trybot stdout (leave empty to use clipboard): ")

  ;; Yank URL from clipboard if necessary.
  (when (= (length url) 0)
    (with-temp-buffer
      (clipboard-yank)
      (setq url (buffer-string))))

  ;; TODO: fixup URL to append /text if necessary.

  ;; Extract the body out of the URL.
  ; TODO: delete HTTP headers somehow.
  (let ((inhibit-read-only t)
        (coding-system-for-read 'utf-8-dos))
    (switch-to-buffer (get-buffer-create "*trybot*"))
    (buffer-swap-text (url-retrieve-synchronously url))

    (trybot-fixup)))

(provide 'trybot)
