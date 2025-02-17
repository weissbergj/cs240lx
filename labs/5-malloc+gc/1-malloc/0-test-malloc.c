// weak test of malloc/free: checks that subsequent allocations are
// <sizeof (void *)> bytes from each other.
#include "test-interface.h"

void notmain(void) {
    enum { ntests = 10 };
    
    char *p0 = kr_malloc(1);
    for(unsigned i = 0; i < ntests; i++)  {
        char *p1 = kr_malloc(1);

        output("malloc(1)=%x, diff=%d\n", (unsigned) kr_malloc(1), (int)(p1 - p0));
        // see that we gok et back the previous.
        kr_free(p1);
        char *p2 = kr_malloc(1);
        if(p1 != p2)
            panic("expected to reallocate freed ptr %p\n", p1);
        p0=p1;
        trace("iter=%d: trivial free/alloc worked as expected\n", i);
    }
    trace("success: malloc/free appear to work on %d tests!\n", ntests);
}
