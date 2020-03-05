;; The mimosa project
; TODO: I do not know if a table is sync safe
; Ill assume yes for now


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
             )

(define-type disk
             ide-device
             disk-mut
             cache
             cache-used
             type
             )

(define (disk-fetch-and-set! disk lba)
  (let ((mut (disk-mut disk))
        (cache (disk-cache disk))
        (used (disk-cache-used disk));; todo: check overflow
        (dev (disk-ide-device disk)))
    (mutex-lock! mut)
    (let* ((raw-vect (ide-read-sectors dev lba 1))
           (sect (create-sector raw-vect lba))) 
      (table-set! cache lba sect)
      (disk-cached-used-set! disk (++ used))
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
     (mutex-unlock! smut)
     #t))

(define (create-sector v lba)
  (make-sector lba #f (make-mutex) v)) 

(define (create-disk dev type)
 (make-disk 
  dev
  (make-mutex)
  (make-table size: DISK-CACHE-MAX-SZ)
  type))
