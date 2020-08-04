;; This file was stolen from the cantor project (https://github.com/KDE/cantor)

(in-package :maxima)
#+clisp (defvar *old-suppress-check-redefinition*
	      custom:*suppress-check-redefinition*)
#+clisp (setf custom:*suppress-check-redefinition* t)
(setf *alt-display2d* 'latex-print)
(setf *alt-display1d* 'regular-print)
(setf *prompt-prefix* "<prompt>")
;;the newline at the end of the next line is needed, as we otherwise might run into
;;weird issues with KPtyProcess caching the line of the prompt, and sending it twice
;;to readStdOut()
(setf *prompt-suffix* "</prompt>
")
;(setf *general-display-prefix* "DISPLAY_PREFIX")
;(setf *maxima-prolog* "Hello World")
;(setf *maxima-epilog* "Bye!")

(setf $display2d t)

;#-gcl(setf *debug-io* (make-two-way-stream *standard-input* *standard-output*))
;#+(or cmu sbcl scl)
;(setf *terminal-io* (make-two-way-stream *standard-input* *standard-output*))

;; Small changes to mactex.lisp for interfacing with TeXmacs
;; Andrey Grozin, 2001-2006

;(defun main-prompt ()
;  (format () "~A(~A~D) ~A" *prompt-prefix*
;    (tex-stripdollar $inchar) $linenum *prompt-suffix*))

(declare-top
	 (special lop rop ccol $gcprint $inchar)
	 (*expr tex-lbp tex-rbp))
(defconstant texport *standard-output*)

(defun tex-stripdollar (x)
  (let ((s (quote-% (maybe-invert-string-case (symbol-name (stripdollar x))))))
    (if (> (length s) 1)
      (concatenate 'string s)
      s)))

(defprop mtimes ("\\*") texsym)


(defun latex-print (x)
  (princ "<result>")
  (princ "<text>")
  (linear-displa x )
  (princ "</text>")

  (let ((ccol 1))
    (mapc #'princ
        (tex x '("<latex>") '("</latex>") 'mparen 'mparen)))

  (princ "</result>")
)

(defun regular-print (x)
  (princ "<result>")
  (princ "<text>")
  (linear-displa x)
  (princ "</text>")
  (princ "</result>")
)

(defun cantor-inspect (var)
  ($disp var)
  (mapc #'(lambda (x)
	    ($disp (eval x))
	    ($disp "-cantor-value-separator-")
	  )
	(cdr var)
	)
)

;; Fix bug with maxima tex output, LaTeX and amsmath, until Maxima team don't solve it
;; More info: https://sourceforge.net/p/maxima/bugs/3432/
;;(defun tex-matrix(x l r)
;;  (append l `("\\begin{pmatrix}")
;;	  (mapcan #'(lambda(y)
;;		      (tex-list (cdr y) nil (list "\\\\ ") "&"))
;;		  (cdr x))
;;	  '("\\end{pmatrix}") r))

#+clisp (setf custom:*suppress-check-redefinition*
	      *old-suppress-check-redefinition*)
