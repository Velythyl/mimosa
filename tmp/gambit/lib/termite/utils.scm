;; utils

;; ----------------------------------------------------------------------------
;; Some basic utilities

(##supply-module termite/utils)
(##namespace ("termite/utils#"))
(##include "/dsk1/gambit/lib/_prim#.scm")

;; make-uuid
(##include "uuid.scm")

(define (filter pred? lst)
  (cond
    ((null? lst) '())

    ((pred? (car lst))
     (cons (car lst)
           (filter pred? (cdr lst))))
    (else
      (filter pred? (cdr lst)))))

(define (remove pred? lst)
  (filter (lambda (x)
            (not (pred? x)))
          lst))

(define (quoted-symbol? datum)
  (and
    (pair? datum)
    (eq? (car datum) 'quote)
    (pair? (cdr datum))
    (symbol? (cadr datum))))

(define (unquoted-symbol? datum)
  (and
    (pair? datum)
    (eq? (car datum) 'unquote)
    (pair? (cdr datum))
    (symbol? (cadr datum))))

