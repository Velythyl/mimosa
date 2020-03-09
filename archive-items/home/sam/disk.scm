;; The mimosa project
; TODO: I do not know if a table is sync safe
; Ill assume yes for now
(define-library (disk)
(import (gambit)
        (ide)
        (utils)
        (debug)
        (low-level))
(export disk-list disk-apply-sector disk-acquire-block disk-release-block init-disks sector-vect)
(begin
  (define DISK-CACHE-MAX-SZ 4098)
  (define DISK-IDE 0)
  (define MAX-NB-DISKS 32)
  (define DISK-LOG2-BLOCK-SIZE 9)

  ; Disk types
  (define DISK-TYPE-IDE 'IDE-DISK)

  (define-type sector
               lba
               dirty?
               mut
               vect
               ref-count
               disk-l
               )

  (define-type disk
               ide-device
               mut
               cache
               cache-used
               type
               )

  (define disk-list (make-list MAX-NB-DISKS 'NO-DISK))

  (define (disk-fetch-and-set! disk lba)
    (let ((mut (disk-mut disk))
          (cache (disk-cache disk))
          (used (disk-cache-used disk));; todo: check overflow
          (dev (disk-ide-device disk)))
      (debug-write "B4 lock")
      (mutex-lock! mut)
      (debug-write "After lock")
      (let* ((raw-vect (ide-read-sectors dev lba 1))
             (sect (create-sector raw-vect lba disk))) 
        (table-set! cache lba sect)
        (disk-cache-used-set! disk (++ used))
        (mutex-unlock! mut)     
        sect)))

  (define (disk-read-sector disk lba)
    (let* ((cache (disk-cache disk))
           (dev (disk-ide-device disk))
           (cached (table-ref cache lba #f)))
      (if cached 
          cached 
          (disk-fetch-and-set! disk lba)))) 

  (define (flush-block disk sector)
    (let ((smut (sector-mut sector))
          (dmut (disk-mut disk))
          (dev (disk-ide-device disk))
          (lba (sector-lba sector))
          (v (sector-vect sector)))
      (mutex-lock! smut)
      (mutex-lock! dmut)
      (ide-write-sectors dev lba v 1)
      (mutex-unlock! dmut)
      (mutex-unlock! smut); todo not sure if necessary
      #t))

  (define (create-sector v lba disk)
    (make-sector 
      lba
      #f
      (make-mutex)
      v 
      0
      (lambda () disk))) 

  (define (create-disk dev type)
    (make-disk 
      dev
      (make-mutex)
      (make-table size: DISK-CACHE-MAX-SZ)
      0
      type))

  (define (disk-acquire-block disk lba)
    (let* ((sector (disk-read-sector disk lba))
           (refs (sector-ref-count sector))
           (mut (sector-mut sector)))
      (mutex-lock! mut)
      (begin
        (sector-ref-count-set! sector (++ refs))
        (mutex-unlock! mut)
        sector)))

  (define (disk-release-block sect)
    (let* ((lba (sector-lba sect))
           (mut (sector-mut sect))
           (disk ((sector-disk-l sect)))
           (refs (sector-ref-count sect)))
      (mutex-lock! mut)
      (begin
        (sector-ref-count-set! sect (- refs 1))
        (mutex-unlock! mut)
        (if (= refs 1) (flush-block disk sect))
        #t)))

  (define (init-disks)
    (let ((disk-idx 0)) 
      (for-each
        (lambda (ctrl)
          (for-each (lambda (dev)
                      (begin 
                        (list-set! disk-list disk-idx (create-disk dev DISK-TYPE-IDE))   
                        (set! disk-idx (++ disk-idx))))
                    (filter (ide-controller-devices ctrl)
                            (o not device-absent?))))
        (vector->list IDE-CTRL-VECT))
      disk-list))

(define (init-disks)
  (let* ((ide-devices (ide#list-devices))
         (zipped (zip disk-list ide-devices)))
    (set! disk-list (map (lambda (e)
                           (if (pair? e)
                               (create-disk (cadr e) DISK-TYPE-IDE)
                               e)) zipped))))

  ; For a disk, a block address, apply function 
  ; fn on the sector vector
  (define (disk-apply-sector dsk lba fn)
    (let* ((sect (disk-acquire-block dsk lba))
           (rslt (fn (sector-vect sect))))
      (disk-release-block sect)
      rslt))
  ))
