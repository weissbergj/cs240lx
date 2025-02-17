// // #error "copy kmalloc over"

// #include "kr-malloc.h"

// #define roundup(x,n) (((x)+((n)-1))&(~((n)-1)))

// union align {
//         double d;
//         void *p;
//         void (*fp)(void);
// };

// typedef union header { /* block header */
// 	struct {
//     		union header *ptr; /* next block if on free list */
//     		unsigned size; /* size of this block */
//   	} s;
//   	union align x; /* force alignment of blocks */
// } Header;

// #define NALLOC 1024

// static Header base;
// static Header *freep = 0;

// static Header *morecore(unsigned nu) {
//     char *cp = sbrk(nu * sizeof(Header));
//     Header *up;

//     if(cp == (char *)-1)
//         return 0;
//     up = (Header *)cp;
//     up->s.size = nu;
//     kr_free((void *)(up + 1));
//     return freep;
// }

// void *kr_malloc(unsigned nbytes) {
//     Header *p, *prevp;
//     unsigned nunits;

//     if(nbytes == 0)
//         return 0;
//     nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;
//     if((prevp = freep) == 0) {
//         base.s.ptr = freep = &base;
//         base.s.size = 0;
//     }
//     for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr) {
//         if(p->s.size >= nunits) {
//             if(p->s.size == nunits) {
//                 prevp->s.ptr = p->s.ptr;
//             } else {
//                 p->s.size -= nunits;
//                 p += p->s.size;
//                 p->s.size = nunits;
//             }
//             freep = prevp;
//             return (void *)(p + 1);
//         }
//         if(p == freep) {
//             if((p = morecore(nunits)) == 0)
//                 return 0;
//         }
//     }
// }

// void kr_free(void *ap) {
//     Header *bp, *p;

//     if(ap == 0)
//         return;
//     bp = (Header *)ap - 1;
//     for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
//         if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
//             break;
//     if(bp + bp->s.size == p->s.ptr) {
//         bp->s.size += p->s.ptr->s.size;
//         bp->s.ptr = p->s.ptr->s.ptr;
//     } else {
//         bp->s.ptr = p->s.ptr;
//     }
//     if(p + p->s.size == bp) {
//         p->s.size += bp->s.size;
//         p->s.ptr = bp->s.ptr;
//     } else {
//         p->s.ptr = bp;
//     }
//     freep = p;
// }
