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
; M-x trybot-test

(defvar chrome-root nil
  "Path to the src/ directory of your Chrome checkout.")

(defun get-chrome-root ()
  (or chrome-root default-directory))

(defun trybot-fixup ()
  "Parse and fixup the contents of the current buffer as trybot output."

  ;; Fixup paths.
  (cd (get-chrome-root))

  ;; Delete Windows \r.
  (delete-trailing-whitespace)

  ;; Fix up path references.
  ; XXX is there something I should so so this stuff doesn't end up on the
  ; undo stack?
  (goto-char (point-min))
  ; Fix Windows paths ("d:\...\src\").
  ; TODO: need to fix case; e.g. third_party/webkit -> third_party/WebKit. :(
  (while (re-search-forward "^.:\\\\.*\\\\src\\\\" nil t)
    (replace-match "")
    ; Line now looks like:
    ;  foo\bar\baz.cc error message here
    ; We want to fixup backslashes in path into forward slashes, without
    ; modifying the error message.
    ; XXX current eats backslashes after the filename; how can I limit it to
    ; changing from current point up to the first space?
    (subst-char-in-region (point) (line-end-position) ?\\ ?/))
  (goto-char (point-min))

  ;; Switch into compilation mode.
  (compilation-mode))

(defun trybot-test ()
  "Load our test data and do the trybot parse on it."
  (interactive)

  (switch-to-buffer (get-buffer-create "*trybot-test*"))
  (let ((inhibit-read-only t))
    (erase-buffer)
    (insert-file-contents-literally
       (concat (get-chrome-root) "tools/emacs/trybot-windows.txt"))

    (trybot-fixup)))

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
  (switch-to-buffer (get-buffer-create "*trybot*"))
  (buffer-swap-text (url-retrieve-synchronously url))

  (trybot-fixup))

(provide 'trybot)
