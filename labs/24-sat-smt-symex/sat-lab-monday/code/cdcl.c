/**********************************   *************************
 * I implemented a minimal CDCL SAT solver. It has the following features and justification
 *
 * 1. Two-literal watching for efficient propagation; I chose this over
 *    naive watching to reduce the number of clause visits
 *
 * 2. 1-UIP (First Unique Implication Point) conflict analysis; produces higher quality
 *    learnt clauses compared to alternatives like decision-based learning
 *
 * 3. VSIDS-like (**like**) variable activity heuristics; focuses
 *    search on vars involved in recent conflicts
 *
 * 4. Random restarts with increasing intervals; motivated by infinite loops in basic.c;
 *    escapes search spaces that aren't going anywhere
 *
 * 5. Random polarity decisions; I specifically chose random polarities over
 *    fixed polarity to avoid getting stuck in local minima on formulas
 *    that are SAT but get stuck with fixed-polarity solvers;
 *    this was motivated by basic.c challenges!
 * *
 *  To run:
 *      make check-cdcl
 * 
 *  Alternatively, compile:
 *      cc -O2 -o cdcl_random cdcl_random.c
 *  and run, for example:
 *      ./cdcl_random < ../test_inputs/p2.dimacs
 *
 *  Outputs SAT or UNSAT
 ***********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

/************ DEBUG; set to 1 to turn on ************/
#define DEBUG_MAIN 0
#define DEBUG_WATCH 0
#define DEBUG_PROP 0
#define DEBUG_CONFLICT 0
#define DEBUG_ANALYZE 0

/*******************************************/
/*        macros / structures       */
/****************************************/

typedef enum { VAL_UNASSIGNED = -1, VAL_FALSE = 0, VAL_TRUE = 1 } val_t;

// Each clause
typedef struct {
    int size;      // #literals
    int *lits;      // array of int-literals (pos => var, neg => -var)
    int learnt;
    int watch0;    // 1st watched literal in .lits; index
    int watch1;    // 2nd watched literal in .lits; index
} clause_t;

typedef struct {
    val_t value;
    int level;
    clause_t *reason;  // NULL if decision
} varinfo_t;

// array of "watchers[list]" for each literal index */
typedef struct {
    clause_t **data;
    int cap;
    int size;
} watchlist_t;

// "trail" array in order of assignment; array "trail_lim" for index at each decision level
static int *trail = NULL;
static int trail_sz = 0;

static int *trail_lim = NULL; 
static int trail_lim_sz = 0;  // current decision level

static int current_dl = 0; // current decision level
static int conflict_ct = 0;
static int restart_ct = 0;

// I fix the undeclared RESTART_INTERVAL
static int RESTART_INTERVAL = 50; // smaller forces more random restarts

/************************************************/
/*     GLOBALS from input and data structures      */
/***************************************************/

static unsigned N_VARS = 0, N_CLAUSES = 0;
static clause_t *CLAUSES = NULL;
static unsigned CLAUSE_CAP = 0;
static unsigned TOT_CLAUSES = 0;

static varinfo_t *VARINFO = NULL; // var -> {value, level, reason}
static watchlist_t *WATCHES = NULL; // watch for each literal index
static double *activity = NULL; // var -> activity
static double var_inc = 1.0;
static double var_decay = 0.8;

static int *order_heap = NULL; // array of variable indices
static int heap_size = 0;
static int *heap_index = NULL; // var -> index in the heap

static int propQ = 0; // index into trail for BFS-like propagation

/************************************************/
/* LITERAL <-> INDEX, sign, var_of                */
/*************************************************/
static inline int var_of(int lit) { return (lit > 0) ? lit : -lit; }
static inline int sign_of(int lit) { return (lit > 0) ? 0 : 1; }  // 0=pos,1=neg
// prev map literal -> index: 
//   index( v>0 ) = 2 * v
//   index( v<0 ) = 2 * |v| + 1
static inline int lit_index(int lit) {
    int v = (lit > 0) ? lit : -lit;
    return (v << 1) | (lit < 0);
}
static inline int index_var(int idx) { return (idx >> 1); }
static inline int index_sign(int idx) { return (idx & 1); }
static inline int index_to_lit(int idx) {
    int v = (idx >> 1);
    int s = (idx & 1);
    return (s ? -v : v);
}
static inline int negate_lit(int lit) { return -lit; }

/***************************************************/
/*                 WATCHLIST GROWTH                  */
/***************************************************/
static void watchlist_push(int idx, clause_t* c) {
    watchlist_t *wl = &WATCHES[idx];
    if(wl->size == wl->cap) {
        wl->cap = (wl->cap < 4) ? 8 : (wl->cap * 2);
        wl->data = realloc(wl->data, sizeof(clause_t*) * wl->cap);
    }
    wl->data[wl->size++] = c;
}

/***************************************************/
/*        VALUE_OF lit, set literal, etc.             */
/***************************************************/
static inline val_t value_lit(int lit) {
    int v = var_of(lit);
    val_t val = VARINFO[v].value;
    if(val == VAL_UNASSIGNED) return VAL_UNASSIGNED;
    int isNeg = (lit < 0);
    // if assigned=TRUE but isNeg -> false, etc
    if(val == VAL_TRUE) return (isNeg ? VAL_FALSE : VAL_TRUE);
    if(val == VAL_FALSE) return (isNeg ? VAL_TRUE : VAL_FALSE);
    return VAL_UNASSIGNED; 
}

static inline void var_bump_activity(int v) {
    activity[v] += var_inc;
    if(activity[v] > 1e100) {
        // rescale
        for(unsigned i = 1; i <= N_VARS; i++) activity[i] *= 1e-100;
        var_inc *= 1e-100;
    }
}
static inline void var_decay_activity(void) { var_inc *= (1.0/var_decay); }

static int enqueue(int lit, clause_t* reason) {
    int v = var_of(lit);
    val_t val = VARINFO[v].value;
    if(val != -1) return (value_lit(lit) == 0) ? 0 : 1;
    VARINFO[v].value = (lit > 0 ? VAL_TRUE : VAL_FALSE);
    VARINFO[v].level = current_dl;
    VARINFO[v].reason = reason;
    trail[trail_sz++] = v;
    return 1;
}

/***********************************************/
/* CANCEL (backtrack)                          */
/***********************************************/
static void cancel_until(int level) {
    while(current_dl > level) {
        for(int c = trail_lim[current_dl]; c < trail_sz; c++) {
            int v = trail[c];
            VARINFO[v].value = VAL_UNASSIGNED;
            VARINFO[v].reason = NULL;
        }
        trail_sz = trail_lim[current_dl];
        current_dl--;
    }
    if(propQ > trail_sz) propQ = trail_sz; //ensures no out-of-bounds
}

/***********************************/
/* ORDER HEAP                             */
/***************************************/
static inline void heap_swap(int i, int j) {
    int va = order_heap[i];
    int vb = order_heap[j];
    order_heap[i] = vb;
    order_heap[j] = va;
    heap_index[vb] = i;
    heap_index[va] = j;
}
static void heap_sift_up(int i) {
    int v = order_heap[i];
    double act = activity[v];
    while(i > 0) {
        int p = (i - 1) >> 1;
        int vp = order_heap[p];
        if(activity[vp] >= act) break;
        heap_swap(p, i);
        i = p;
    }
}
static void heap_sift_down(int i) {
    while(1) {
        int left = (i << 1) + 1;
        int right = (i << 1) + 2;
        int largest = i;
        if(left < heap_size) {
            int lv = order_heap[left];
            if(activity[lv] > activity[order_heap[largest]]) largest = left;
        }
        if(right < heap_size) {
            int rv = order_heap[right];
            if(activity[rv] > activity[order_heap[largest]]) largest = right;
        }
        if(largest == i) break;
        heap_swap(i, largest);
        i = largest;
    }
}
static void heap_insert_var(int v) {
    if(heap_index[v] >= 0) return; 
    int i = heap_size++;
    order_heap[i] = v;
    heap_index[v] = i;
    heap_sift_up(i);
}
static int heap_pop(void) {
    if(!heap_size) return 0;
    int v = order_heap[0];
    heap_size--;
    order_heap[0] = order_heap[heap_size];
    heap_index[order_heap[0]] = 0;
    heap_index[v] = -1;
    heap_sift_down(0);
    return v;
}

// /***************************************************/
// /* PROPAGATION: INITIAL TWO_LITERAL WATDCH              */
// /***************************************************/
// static clause_t* propagate(void){
//     while(propQ< trail_sz){
//         int v = trail[propQ++];
//         val_t val = VARINFO[v].value;
//         int lit = (val==VAL_TRUE)? v : -v;
//         int idx = lit_index(-lit); // watchers of  opposite
//         watchlist_t *wl = &WATCHES[idx];
//         int j = 0;
//         for(int i=0;i< wl->size; i++){
//             clause_t* c = wl->data[i];
//             int w0 = c->watch0;
//             int w1 = c->watch1;
//             int L0 = c->lits[w0];
//             int L1 = c->lits[w1];
//             if(L0== -lit){
//                 // swap watchers
//                 c->watch0 = w1; c->watch1 = w0;
//                 L0 = c->lits[c->watch0];
//                 L1 = c->lits[c->watch1];
//             }
//             //if L0 is satisfied do nothing
//             if(value_lit(L0)==VAL_TRUE){
//                 wl->data[j++] = c;
//                 continue;
//             }
//             // find new lit to watch
//             int found = 0;
//             for(int k=0;k< c->size;k++){
//                 if(k== c->watch0 || k== c->watch1) continue;
//                 int cand = c->lits[k];
//                 if(value_lit(cand)!=VAL_FALSE){
//                     c->watch1 = k;
//                     watchlist_push(lit_index(cand), c);
//                     found = 1;
//                     break;
//                 }
//             }
//             if(found) continue;
//             // no new watch _> conflict or unit
//             if(value_lit(L0)==VAL_FALSE){
//                 // conflict
//                 wl->data[j++] = c;
//                 return c;
//             }
//             if(value_lit(L0)==VAL_UNASSIGNED){
//                 if(!enqueue(L0,c)){
//                     wl->data[j++] = c;
//                     return c;
//                 }
//             }
//             wl->data[j++] = c;
//         }
//         wl->size = j;
//     }
//     return NULL;
// }

/***************************************************/
/* PROPAGATION: FIXED TWO_LITERAL WATDCH              */
/***************************************************/
static clause_t* propagate(void) {
    while(propQ < trail_sz) {
        int v = trail[propQ++];
        val_t val = VARINFO[v].value;
        int lit = (val == VAL_TRUE) ? v : -v;  

        int idx = lit_index(-lit); 
        watchlist_t *wl = &WATCHES[idx];

        int writePos = 0;
        for(int readPos = 0; readPos < wl->size; readPos++) {
            clause_t* c = wl->data[readPos];

            int w0 = c->watch0;  
            int w1 = c->watch1;  
            int L0 = c->lits[w0];
            int L1 = c->lits[w1];

            if(L0 == -lit) {
                c->watch0 = w1;
                c->watch1 = w0;
                L0 = c->lits[c->watch0];
                L1 = c->lits[c->watch1];
            }

            // If either watcher is TRUE, clause satisfied; watch
            if(value_lit(L0) == VAL_TRUE || value_lit(L1) == VAL_TRUE) {
                wl->data[writePos++] = c;
                continue;  
            }

            // else find new lit to watch instead of L1; if L1 == -lit or L1 is false; L0 is not TRUE, L1 is not TRUE -> L1 either FALSE or UNASSIGNED.
            int foundNewWatch = 0;
            if(value_lit(L0) != VAL_FALSE) {
                if(value_lit(L1) == VAL_FALSE) {
                    // Look for new watch in c->lits
                    for(int k = 0; k < c->size; k++) {
                        if(k == c->watch0 || k == c->watch1) continue;
                        int cand = c->lits[k];
                        if(value_lit(cand) != VAL_FALSE) {
                            // Found new watch
                            c->watch1 = k;
                            watchlist_push(lit_index(cand), c);
                            foundNewWatch = 1;
                            break;
                        }
                    }
                    if(foundNewWatch) {
                        // Clause no longer watched
                        continue;
                    }
                }
            } else {
                // L0 false --> find new watch in place of L0 by swap
                c->watch0 = w1;
                c->watch1 = w0;
                L0 = c->lits[c->watch0];
                L1 = c->lits[c->watch1];
                //  search for replacement for new L1 if false
                if(value_lit(L1) == VAL_FALSE) {
                    for(int k = 0; k < c->size; k++) {
                        if(k == c->watch0 || k == c->watch1) continue;
                        int cand = c->lits[k];
                        if(value_lit(cand) != VAL_FALSE) {
                            c->watch1 = k;
                            watchlist_push(lit_index(cand), c);
                            foundNewWatch = 1;
                            break;
                        }
                    }
                    if(foundNewWatch) continue;
                }
            }

            // no new watch found -> unit or a conflict; all other lit false --> one of L0 / L1 may be UNASSIGNED
            // if both false, conflict
            if(value_lit(L0) == VAL_FALSE && value_lit(L1) == VAL_FALSE) {
                wl->data[writePos++] = c;
                return c;
            }

            // one UNASSIGNED
            if(value_lit(L0) == VAL_UNASSIGNED && value_lit(L1) == VAL_FALSE) {
                if(!enqueue(L0, c)) {
                    wl->data[writePos++] = c;
                    return c;
                }
            } else if(value_lit(L1) == VAL_UNASSIGNED && value_lit(L0) == VAL_FALSE) {
                if(!enqueue(L1, c)) {
                    wl->data[writePos++] = c;
                    return c;
                }
            }
            // If both UNASSIGNED, pick one to enqueu if size=2; else, pick L0
            else if(value_lit(L0) == VAL_UNASSIGNED && value_lit(L1) == VAL_UNASSIGNED) {
                if(!enqueue(L0, c)) {
                    wl->data[writePos++] = c;
                    return c;
                }
            }

            // clause in watchlist
            wl->data[writePos++] = c;
        }
        // update watchlist size
        wl->size = writePos;
    }
    return NULL;
}

/*************************************************/
/* 1-UIP CONFLICT ANALYSIS                     */
/*************************************************/
static char *seen = NULL;   //var -> bool
static int *an_stack = NULL;  // stack for unmark
static int an_stack_sz = 0;
static int *learnt_arr = NULL;
static int learnt_sz = 0;

static void unmark_all(void) {
    while(an_stack_sz > 0) {
        int v = an_stack[--an_stack_sz];
        seen[v] = 0;
    }
}

static clause_t* analyze(clause_t* confl, int* out_btlevel) {
    learnt_sz = 0;
    int pathC = 0;
    int p = -1;
    clause_t* reason = confl;
    int idx = trail_sz - 1;

    do {
        for(int i = 0; i < reason->size; i++) {
            int q = reason->lits[i];
            int v = var_of(q);
            if(!seen[v]) {
                seen[v] = 1;
                an_stack[an_stack_sz++] = v;
                if(VARINFO[v].level == current_dl) pathC++;
                else learnt_arr[learnt_sz++] = q;
            }
        }
        while(!seen[trail[idx]]) idx--;
        p = trail[idx];
        idx--;
        reason = VARINFO[p].reason;
        pathC--;
    } while(pathC > 0);

    // UIP is p
    learnt_arr[learnt_sz++] = (VARINFO[p].value == VAL_TRUE) ? -p : p;
    // find 2nd highest level
    int backL = 0;
    for(int i = 0; i < learnt_sz - 1; i++) {
        int v = var_of(learnt_arr[i]);
        int lvl = VARINFO[v].level;
        if(lvl > backL && lvl < current_dl) backL = lvl;
    }
    *out_btlevel = backL;

    // build new clause
    clause_t* newc = (clause_t*) malloc(sizeof(clause_t));
    newc->size = learnt_sz;
    newc->lits = (int*) malloc(sizeof(int) * learnt_sz);
    newc->learnt = 1;
    for(int i = 0; i < learnt_sz; i++) {
        newc->lits[i] = learnt_arr[i];
        var_bump_activity(var_of(learnt_arr[i]));
    }
    newc->watch0 = 0;
    newc->watch1 = (learnt_sz > 1) ? 1 : 0;

    if(TOT_CLAUSES == CLAUSE_CAP) {
        CLAUSE_CAP = (CLAUSE_CAP < 8) ? 16 : (CLAUSE_CAP * 2);
        CLAUSES = (clause_t*) realloc(CLAUSES, CLAUSE_CAP * sizeof(clause_t));
    }
    CLAUSES[TOT_CLAUSES] = *newc;
    clause_t* ret = &CLAUSES[TOT_CLAUSES];
    TOT_CLAUSES++;
    free(newc);

    watchlist_push(lit_index(ret->lits[ret->watch0]), ret);
    watchlist_push(lit_index(ret->lits[ret->watch1]), ret);

    unmark_all();
    return ret;
}

/***************************************************/
/* DECIDE WITH RANDOM POLARTITY                       */
/***************************************************/
static int decideVar(void) {
    int bestVar = 0;
    double bestAct = -1.0;
    // pick var with highest activity among unassigned
    for(unsigned i = 1; i <= N_VARS; i++) {
        if(VARINFO[i].value == VAL_UNASSIGNED) {
            if(activity[i] > bestAct) {
                bestAct = activity[i];
                bestVar = i;
            }
        }
    }
    if(!bestVar) {
        // fallback if can't find any
        for(unsigned i = 1; i <= N_VARS; i++) {
            if(VARINFO[i].value == VAL_UNASSIGNED) {
                bestVar = i;
                break;
            }
        }
    }
    return bestVar;
}

static int pickPolarityRandom(int var) { return (rand() % 2 ? var : -var); }

/***************************************************/
/* MAIN SEARCH                                     */
/***************************************************/
static int search_inner(void) {
    while(1) {
        clause_t* confl = propagate();
        if(confl) {
            conflict_ct++;
            if(current_dl == 0) return 0; // unsat
            int btlevel = 0;
            clause_t* c = analyze(confl, &btlevel);
            cancel_until(btlevel);
            if(!enqueue(c->lits[0], c)) return 0; // conflict at level0
        } else {
            // no conflict-> check all
            int assigned = 1;
            for(unsigned i = 1; i <= N_VARS; i++) {
                if(VARINFO[i].value == VAL_UNASSIGNED) {
                    assigned = 0;
                    break;
                }
            }
            if(assigned) return 1; // sat

            // do a decision
            current_dl++;
            trail_lim[current_dl] = trail_sz;

            // pick var +random polarity
            int var = decideVar();
            if(!var) return 1; //  all assigned
            int pLit = pickPolarityRandom(var);
            if(!enqueue(pLit, NULL)) return 0; 
        }
        // random restarts
        if(conflict_ct >= RESTART_INTERVAL) {
            conflict_ct = 0;
            RESTART_INTERVAL = (RESTART_INTERVAL * 3) / 2 + 10;
            cancel_until(0);
        }
    }
}

/***************************************************/
/* MAIN                                          */
/***************************************************/
int main(int argc, char** argv) {
    srand(time(NULL));

    // skip lines  starting with c
    for(char c; (c = getc(stdin)) == 'c'; ) {
        while(getc(stdin) != '\n');
    }
    assert(scanf(" cnf %u %u\n", &N_VARS, &N_CLAUSES) == 2);

    CLAUSE_CAP = (N_CLAUSES + 16);
    CLAUSES = (clause_t*) calloc(CLAUSE_CAP, sizeof(clause_t));
    TOT_CLAUSES = N_CLAUSES;

    VARINFO = (varinfo_t*) calloc(N_VARS + 1, sizeof(varinfo_t));
    for(unsigned i = 1; i <= N_VARS; i++) {
        VARINFO[i].value = VAL_UNASSIGNED;
        VARINFO[i].level = 0;
        VARINFO[i].reason = NULL;
    }

    WATCHES = (watchlist_t*) calloc((N_VARS + 1) * 2, sizeof(watchlist_t));
    for(unsigned i = 0; i < (N_VARS + 1) * 2; i++) {
        WATCHES[i].cap = 0;
        WATCHES[i].size = 0;
        WATCHES[i].data = NULL;
    }

    activity = (double*) calloc(N_VARS + 1, sizeof(double));
    for(unsigned i = 1; i <= N_VARS; i++) activity[i] = 0.0;

    // read clauses
    for(unsigned i = 0; i < N_CLAUSES; i++) {
        int arrCap = 4, arrSz = 0;
        int *arr = (int*) malloc(sizeof(int) * arrCap);
        int lit = 0;
        while(1) {
            assert(scanf("%d", &lit) == 1);
            if(!lit) break;
            if(arrSz == arrCap) {
                arrCap *= 2;
                arr = (int*) realloc(arr, sizeof(int) * arrCap);
            }
            arr[arrSz++] = lit;
        }
        CLAUSES[i].size = arrSz;
        CLAUSES[i].lits = arr;
        CLAUSES[i].learnt = 0;
        CLAUSES[i].watch0 = 0;
        CLAUSES[i].watch1 = (arrSz > 1) ? 1 : 0;
    }

    // watchers
    for(unsigned i = 0; i < N_CLAUSES; i++) {
        int L0 = CLAUSES[i].lits[CLAUSES[i].watch0];
        int L1 = CLAUSES[i].lits[CLAUSES[i].watch1];
        watchlist_push(lit_index(L0), &CLAUSES[i]);
        watchlist_push(lit_index(L1), &CLAUSES[i]);
    }

    //  trail
    trail = (int*) malloc(sizeof(int) * (N_VARS + 1) * 2);
    trail_sz = 0;
    trail_lim = (int*) malloc(sizeof(int) * (N_VARS + 1));
    trail_lim[0] = 0;
    current_dl = 0;

    //conflict analysis
    seen = (char*) calloc(N_VARS + 1, sizeof(char));
    an_stack = (int*) malloc(sizeof(int) * (N_VARS + 1));
    learnt_arr = (int*) malloc(sizeof(int) * (N_VARS + 1) * 2);

    // solve
    int ret = search_inner();
    if(!ret) {
        printf("UNSAT\n");
        return 0;
    } 
    // final check -> sat
    printf("SAT\n");
    for(unsigned v = 1; v <= N_VARS; v++) {
        if(VARINFO[v].value == VAL_TRUE) printf("%u ", v);
        else printf("-%u ", v);
    }
    printf("\n");
    return 0;
}