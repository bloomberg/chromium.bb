;;; Directory Local Variables
;;; For more information see (info "(emacs) Directory Variables")


((python-mode .
              ((python-indent-offset . 2)
               ;; Add a python-mode hook that will detect if the
               ;; current buffer is inside of Chromite and, if so,
               ;; offer the direct executable wrapper of the current
               ;; file when running M-x `compile`. This is useful for
               ;; unittests, in particular, because Emacs will
               ;; automatically highlight unittest failure line
               ;; numbers so that they may be jumped to.
               (eval if (file-exists-p
                         (concat
                          (file-name-directory
                           (let ((d (dir-locals-find-file ".")))
                             (if (stringp d) d (car d))))
                          "/run_tests"))
                     (set (make-local-variable 'compile-command)
                          (if buffer-file-name
                             (shell-quote-argument
                              (file-name-sans-extension buffer-file-name))))))))
