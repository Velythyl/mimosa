;; The mimosa project
(define-library (utils)
                (import (gambit))
                (export 
                  ++
                  >>
                  <<
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
                  )
    (begin
      (define (<< n shl)
        (fxarithmetic-shift n shl))

      (define (>> n shr)
        (fxarithmetic-shift-right n shr))

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


      )) 
