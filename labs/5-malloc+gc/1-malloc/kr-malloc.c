/*
 * forced-kr-alloc.c
 * A forced "K&R" style allocator + custom sbrk in one file,
 * with unique symbol names to avoid conflicts with other code.
 */

#include "rpi.h"  // for putk

/****************************************************************
 * Minimal integer-to-string for debugging prints.
 ****************************************************************/
static void u2str(unsigned x, char *buf) {
    if(x == 0) {
        buf[0] = '0';
        buf[1] = 0;
        return;
    }
    int i=0;
    while(x > 0) {
        buf[i++] = '0' + (x % 10);
        x /= 10;
    }
    buf[i] = 0;
    for(int j=0; j < i/2; j++) {
        char tmp = buf[j];
        buf[j] = buf[i-1-j];
        buf[i-1-j] = tmp;
    }
}
static void putu(unsigned x) {
    char buf[32];
    u2str(x, buf);
    // putk(buf);
}

/*************************************************************
 * 1) A "my_sbrk" that uses a high memory region,
 * out of the way of .data and .bss.
 *************************************************************/
void *my_sbrk(long increment) {
    // putk("my_sbrk: start\n");
    if(increment < 0) {
        // putk("my_sbrk: negative increment => -1\n");
        return (void*)-1;
    }

    // put the "heap" far above 0x8000
    // e.g., 256KB up so we don't overlap .data or .bss
    static char *heap_start = (char *)0x40000;  // 256KB in
    static char *heap_ptr   = (char *)0x40000; 
    static char *heap_end   = (char *)(0x40000 + 64*1024); // 64KB chunk

    // putk("my_sbrk: heap_ptr=");
    putu((unsigned)heap_ptr);
    // putk(" inc=");
    putu((unsigned)increment);
    // putk("\n");

    if(heap_ptr + increment > heap_end) {
        // putk("my_sbrk: out of mem => -1\n");
        return (void*)-1;
    }

    char *old = heap_ptr;
    heap_ptr += increment;
    // putk("my_sbrk: returning old=");
    putu((unsigned)old);
    // putk(" new heap_ptr=");
    putu((unsigned)heap_ptr);
    // putk("\n");
    return old;
}

/*************************************************************
 * 2) K&R data structures, forced into a special section to
 *    reduce the chance they get overwritten.
 *************************************************************/
__attribute__((section(".mydata")))
static union header {
    struct {
        union header *ptr;
        unsigned size;
    } s;
    long x;  // alignment
} base;  // "base" sentinel

__attribute__((section(".mydata")))
static union header *freep = 0;

/*************************************************************
 * 3) "my_morecore" calls "my_sbrk" to get more memory.
 *************************************************************/
static union header *my_morecore(unsigned nu) {
    // putk("my_morecore: start\n");
    char *cp = my_sbrk(nu * sizeof(union header));
    if(cp == (char*)-1) {
        // putk("my_morecore: sbrk = -1 => out\n");
        return 0;
    }

    union header *up = (union header *)cp;
    up->s.size = nu;

    // putk("my_morecore: about to call my_free\n");
    // insert new chunk
    extern void kr_free(void *ap); // forward decl
    kr_free((void *)(up + 1));

    // putk("my_morecore: done => returning freep\n");
    return freep;
}

/*************************************************************
 * 4) "my_malloc" with forced re-init if we detect corruption.
 *************************************************************/
void *kr_malloc(unsigned nbytes) {
    // putk("my_malloc: start\n");

    union header *p, *prevp;
    unsigned nunits;

    if(nbytes == 0) {
        // putk("my_malloc: 0 => return 0\n");
        return 0;
    }
    // Round up
    nunits = (nbytes + sizeof(union header) - 1)/sizeof(union header) + 1;

    // forced re-check: if freep is out of range or zero, re-init
    // (In case something overwrote the base block)
    if(!freep || (unsigned)(void*)freep < 0x8000 || (unsigned)(void*)freep > 0x8000000) {
        // putk("my_malloc: forced re-init of base\n");
        base.s.ptr = &base;
        base.s.size= 0;
        freep      = &base;

        // putk("   base=");
        putu((unsigned)&base);
        // putk(" base.s.ptr=");
        putu((unsigned)base.s.ptr);
        // putk(" size=");
        putu(base.s.size);
        // putk("\n");
    }

    if((prevp = freep) == 0) {
        // putk("my_malloc: freep=0 => init base\n");
        base.s.ptr = &base;
        base.s.size=0;
        freep=&base;
    }

    // search
    for(p = prevp->s.ptr;; prevp = p, p = p->s.ptr) {
        // putk("my_malloc loop: p=");
        putu((unsigned)p);
        // putk(" p->s.ptr=");
        putu((unsigned)p->s.ptr);
        // putk(" freep=");
        putu((unsigned)freep);
        // putk(" p->s.size=");
        putu(p->s.size);
        // putk("\n");

        if(p->s.size >= nunits) {
            // block is big enough
            if(p->s.size == nunits) {
                prevp->s.ptr = p->s.ptr;
            } else {
                p->s.size -= nunits;
                p += p->s.size;
                p->s.size = nunits;
            }
            freep = prevp;
            // putk("my_malloc: found block => return\n");
            return (void *)(p+1);
        }
        if(p == freep) {
            // wrapped around
            // putk("my_malloc: wrap => call my_morecore\n");
            if((p = my_morecore(nunits)) == 0) {
                // putk("my_malloc: out of memory\n");
                return 0;
            }
        }
    }
}

/*************************************************************
 * 5) "my_free" merges blocks.
 *************************************************************/
void kr_free(void *ap) {
    // putk("my_free: start\n");
    if(!ap) {
        // putk("my_free: null => return\n");
        return;
    }

    union header *bp = (union header *)ap - 1;
    // putk("my_free: block=");
    putu((unsigned)bp);
    // putk(" size=");
    putu(bp->s.size);
    // putk("\n");

    // If base is messed up, re-init
    if(!freep || (unsigned)(void*)freep < 0x8000 || (unsigned)(void*)freep > 0x8000000) {
        // putk("my_free: forced re-init of base\n");
        base.s.ptr = &base;
        base.s.size=0;
        freep=&base;
    }

    union header *p;
    // insert in circular list
    for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr) {
        // wrapped around
        if(p >= p->s.ptr && (bp > p || bp < p->s.ptr)) {
            break;
        }
    }

    // coalesce up
    if(bp + bp->s.size == p->s.ptr) {
        // putk("my_free: coalesce upper\n");
        bp->s.size += p->s.ptr->s.size;
        bp->s.ptr = p->s.ptr->s.ptr;
    } else {
        bp->s.ptr = p->s.ptr;
    }

    // coalesce down
    if(p + p->s.size == bp) {
        // putk("my_free: coalesce lower\n");
        p->s.size += bp->s.size;
        p->s.ptr = bp->s.ptr;
    } else {
        p->s.ptr = bp;
    }
    freep = p;

    // putk("my_free: done => freep=");
    putu((unsigned)freep);
    // putk("\n");
}
