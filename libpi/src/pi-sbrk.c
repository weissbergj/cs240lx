// #include "rpi.h"        // for panic, putk
// #include "kr-malloc.h"  // for the sbrk prototype

// // Minimal integer-to-string for putk
// static void u2str(unsigned x, char *buf) {
//     if(x == 0) {
//         buf[0] = '0'; 
//         buf[1] = 0;
//         return;
//     }
//     int i=0;
//     while(x > 0) {
//         buf[i++] = '0' + (x % 10);
//         x /= 10;
//     }
//     buf[i] = 0;
//     // reverse in-place
//     for(int j=0; j < i/2; j++) {
//         char tmp = buf[j];
//         buf[j] = buf[i-1-j];
//         buf[i-1-j] = tmp;
//     }
// }

// // Helper to print an unsigned integer with putk
// static void putu(unsigned x) {
//     char buf[32];
//     u2str(x, buf);
//     putk(buf);
// }

// void *sbrk(long increment) {
//     putk("sbrk: start\n");
    
//     // always good to handle negative increments
//     if(increment < 0) {
//         putk("sbrk: negative increment => returning -1\n");
//         return (void*)-1;
//     }

//     static char heap[64 * 1024];    // static array for our "heap"
//     static char *heap_ptr = heap;

//     // debug: print current heap_ptr address as an integer
//     putk("sbrk: current heap_ptr=");
//     putu((unsigned)heap_ptr);
//     putk(" increment=");
//     putu((unsigned)increment);
//     putk("\n");

//     // check for out-of-bounds
//     if(heap_ptr + increment > heap + sizeof(heap)) {
//         putk("sbrk: out of memory => returning -1\n");
//         return (void*)-1;
//     }

//     char *old = heap_ptr;
//     heap_ptr += increment;

//     // print the new pointer
//     putk("sbrk: returning old=");
//     putu((unsigned)old);
//     putk(" new heap_ptr=");
//     putu((unsigned)heap_ptr);
//     putk("\n");

//     // done
//     return old;
// }
