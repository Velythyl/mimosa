;; The mimosa project

(define (bcd->binary byte)
 (+ (* 10 (fxarithmetic-shift-right byte 4)) (fxand byte #x0F)))


(define (<< n shl)
 (fxarithmetic-shift n shl))

(define (>> n shr)
 (fxarithmetic-shift-right n shr))
