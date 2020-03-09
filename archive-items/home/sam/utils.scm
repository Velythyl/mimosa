;; The mimosa project
(define-library (utils)
                (import (gambit))
                (export 
                  ++
                  >>
                  <<
                  s>>
                  s<<
                  O
                  TIME-UNIT-MICROSECS
                  TIME-UNIT-MS
                  TIME-UNIT-NSECS
                  TIME-UNIT-SECONDS 
                  assock 
                  assocv
                  b-chop
                  build-vector
                  filter
                  fxhalve
                  gambit-set-repl-channels!
                  lwrap
                  mask
                  o
                  string-trim
                  until-has-elapsed
                  zip
                  flatten
                  TODO
                  ilog2
                  ; define-struct-fill
                  )
    (begin
      (define (<< n shl)
        (fxarithmetic-shift n shl))

      (define (>> n shr)
        (fxarithmetic-shift-right n shr))

      (define (s<< n shl)
        (arithmetic-shift n shl))

      (define (s>> n shr)
        (arithmetic-shift-right n shr))

      (define (TODO)
       (begin
         (display "STUB")
         #t))


      (define (assock key tbl)
        (car (assoc key tbl)))

      (define (assocv key tbl)
        (cdr (assoc key tbl)))

      ; Combine two functions (\circ)
      (define (o g f)
        (lambda (n)
          (g (f n))))

      ; Combine many functions (\circ with many parameters)
      (define (O fns)
        (lambda (n)
          (if (= (length fns) 0)
              n
              ((o (car fns) (O (cdr fns))) n))))

      (define (incn n)
        (lambda (k)
          (+ k n)))

      (define ++ (incn 1))

      (define CURRENT-THREAD-CHANNELS-ADDR 28)

      ; Set the channels on the thread's current repl
      ; it takes three channels (in, out, err)
      (define (gambit-set-repl-channels! chan1 chan2 chan3)
        (##vector-set! (current-thread) CURRENT-THREAD-CHANNELS-ADDR 
         (##make-repl-channel-ports chan1 chan2 chan3)))

      (define (nanoseconds->time nsecs)
        (let ((seconds (* nsecs 1e-9)))
          (seconds->time seconds)))

      (define (microseconds->time usecs)
        (nanoseconds->time (* 1000 usecs)))

      (define (milliseconds->time msecs)
        (let ((seconds (* msecs 1e-3)))
          (seconds->time seconds)))

      ; Build a vector according to a procedure
      (define (build-vector sz proc)
        (list->vector (map proc (iota sz))))

      ; Remove spaces out of a list
      (define (list-trim msg)
        (if (= (length msg) 0)
            (list)
            (let ((f (car msg)))
              (if (eq? f #\space)
                  (list-trim (cdr msg))
                  (cons f (list-trim (cdr msg)))))))

      ; It's not trim, it's removing spaces
      (define (string-trim msg)
        (list->string (list-trim (string->list msg))))

      ; Why does this not work...
      (define-macro (lwrap expr) 
                    `(lambda () ,expr))

      (define TIME-UNIT-SECONDS seconds->time)
      (define TIME-UNIT-NSECS nanoseconds->time)
      (define TIME-UNIT-MICROSECS microseconds->time)
      (define TIME-UNIT-MS milliseconds->time)

      (define (until-has-elapsed n unit)
        (let ((now (time->seconds (current-time)))
              (unit-secs (time->seconds (unit n))))
          (seconds->time (+ now unit-secs))))

      (define (mask v m)
        (fx> (fxand v m) 0))

      (define (b-chop v)
        (fxand 255 v))

      (define (fxhalve n)
        (fxarithmetic-shift-right n 1))

      (define (filter lst predicate)
        (fold-right (lambda (e r)
                      (if (predicate e)
                          (cons e r)
                          r))
                    (list) ; base
                    lst
                    ))

      (define (zip a b)
        (if (null? a)
            (list)
            (if (null? b)
                (cons (car a) (zip (cdr a) b))
                (let ((ea (car a))
                      (eb (car b))
                      (ra (cdr a))
                      (rb (cdr b)))
                  (cons (list ea eb) (zip ra rb))
                  ))))

      (define flatten
        (lambda (ipt)
          (if (null? ipt)
              '()
              (let ((c (car ipt)))
                (if (pair? c)
                    (flatten c)
                    (cons c (flatten (cdr ipt))))))))
      
      (define (ilog2aux n tot)
       (if (= n 1) tot (ilog2aux (floor (/ n 2)) (++ tot))))

      (define (ilog2 n)
       (ilog2aux n 0))

      (define (bcd->binary byte)
        (+ (* 10 (fxarithmetic-shift-right byte 4)) (fxand byte #x0F)))

      ; (define-macro (define-struct-fill name fields)
      ;               (let ((fill-struct (string-append "fill-" (symbol->string name)))
      ;                     (make-struct (string-append "make-" (symbol->string name)))
      ;                     (vect-idx 0))
      ;                 (begin
      ;                   (define flatten
      ;                     (lambda (ipt)
      ;                       (if (null? ipt)
      ;                           '()
      ;                           (let ((c (car ipt)))
      ;                             (if (pair? c)
      ;                                 c
      ;                                 (cons c (flatten (cdr ipt))))))))

      ;                   (list 'define (list (string->symbol fill-struct) 'vec)
      ;                         (cons 
      ;                           (string->symbol make-struct)
      ;                           (map (lambda (extract)
      ;                                  (let ((offset vect-idx)
      ;                                        (next-offset (+ vect-idx extract)))
      ;                                    (set! vect-idx next-offset) 
      ;                                    (if (<= extract 4)
      ;                                        (cons 'fxior (map (lambda (i)
      ;                                                            (list 'fxarithmetic-shift
      ;                                                                  (list 'vector-ref 'vec (+ offset i))
      ;                                                                  (* i 8)
      ;                                                                  )) (iota extract)))
                                             
      ;                                        (list 'build-vector extract  
      ;                                              (list 'lambda (list 'i) (list 'vector-ref 'vec 'i)))
      ;                                        )))
      ;                                fields))))))
      )) 
