; The MIMOSA project
; Scheme executor bridge
(include "/dsk1/gambit/lib/_asm#.scm")
(include "/dsk1/gambit/lib/_x86#.scm")
(include "/dsk1/gambit/lib/_codegen#.scm")
(include "queue.scm")

(load "utils.scm")
(load "os_types.scm")
(load "keyboard.scm")
(load "int_handle.scm")


(define SHARED-MEMORY-AREA #x300000)
(define EMPTY-BUFFER-ACTION #xAA)
(define GAMBIT-INT-WITH-ARG #xAB)

;;;----------------------------------------------------

;; Convert a u8vector containing machine code into a
;; Scheme procedure taking 0 to 3 arguments.  Calling
;; the Scheme procedure will execute the machine code
;; using the C calling convention.

(define (u8vector->procedure code)
  (let ((mcb (##make-machine-code-block code)))
    (lambda (#!optional (arg1 0) (arg2 0) (arg3 0))
      (##machine-code-block-exec mcb arg1 arg2 arg3))))

;; Create a new code generation context.  The format of
;; the resulting assembly code listing can also be
;; specified, either 'nasm, 'gnu, or #f (no listing,
;; which is the default).

(define (make-cgc #!optional (format #f))
  (let ((cgc (make-codegen-context)))
    (asm-init-code-block cgc 0 endianness)
    (codegen-context-listing-format-set! cgc format)
    (x86-arch-set! cgc arch)
    cgc))

(define arch 'x86-32)
(define endianness 'le)

(define (asm gen #!optional (format #f))
  (let ((cgc (make-cgc format)))
    (gen cgc)
    (let ((code (asm-assemble-to-u8vector cgc)))
      (if format
          (asm-display-listing cgc (current-error-port) #t))
      (u8vector->procedure code))))

;;;----------------------------------------------------

;; Implement interface to IN and OUT x86 instructions

(define inb ;; parameter: port number
  (asm
   (lambda (cgc)
     (x86-mov   cgc (x86-edx) (x86-mem 4 (x86-esp)))
     (x86-sar   cgc (x86-edx) (x86-imm-int 2))
     (x86-mov   cgc (x86-eax) (x86-imm-int 0))
     (x86-in-dx cgc (x86-al))
     (x86-shl   cgc (x86-eax) (x86-imm-int 2))
     (x86-ret   cgc)
     )))

(define outb ;; parameters: value and port number
  (asm
   (lambda (cgc)
     (x86-mov   cgc (x86-edx) (x86-mem 8 (x86-esp)))
     (x86-sar   cgc (x86-edx) (x86-imm-int 2))
     (x86-mov   cgc (x86-eax) (x86-mem 4 (x86-esp)))
     (x86-sar   cgc (x86-eax) (x86-imm-int 2))
     (x86-out-dx cgc (x86-al))
     (x86-shl   cgc (x86-eax) (x86-imm-int 2))
     (x86-ret   cgc)
     )))

(define enable-interrupts
 (asm
  (lambda (cgc)
   (x86-sti cgc))))

(define disable_interrupts
 (asm
  (lambda (cgc)
   (x86-cli cgc))))

(define RTC_PORT_ADDR #x70)
(define RTC_PORT_DATA #x71)

(define RTC_SEC  0)
(define RTC_MIN  2)
(define RTC_HOUR 4)

(define (get-RTC_SEC)
  (outb RTC_SEC RTC_PORT_ADDR) ;; select seconds reg
  (inb RTC_PORT_DATA))         ;; read the register


;;;----------------------------------------------------
;;;                 INTERRUPT HANDLING 
;;;----------------------------------------------------

(define (fact n) (if (= n 1)
                     1
                     (* n (fact (- n 1)))))


(define unhandled-interrupts (make 255))

(define (mimosa-interrupt-handler)
  (let ((interrupt-val (read-iu8 #f SHARED-MEMORY-AREA))
        (int (read-iu8 #f (+ SHARED-MEMORY-AREA 1)))
        (arg (read-iu8 #f (+ SHARED-MEMORY-AREA 2))))
    (let ((packed (list int arg)))
     (push (cons packed unhandled-interrupts) unhandled-interrupts))))

;;;----------------------------------------------------
;;;                  INTERRUPT WIRING 
;;;----------------------------------------------------


(##interrupt-vector-set! 5 mimosa-interrupt-handler)


;;;----------------------------------------------------
;;;              INTERRUPT EXEC ROUTINE 
;;;----------------------------------------------------

(define (exec)
  (if (not (empty? unhandled-interrupts))
      (let ((packed (pop unhandled-interrupts)))
        (display "a")
        (handle-int-with-arg (car packed) (cadr packed))
        (exec))
      ; Spin loop for now? Probably gonna be worth
      ; using thread mecanisms
      (exec)))

(thread-start! (make-thread exec "int execution g-tread"))
