/************************************************************
 * Minimal CDCL SAT Solver with:
 *  - Two-literal watching
 *  - 1-UIP conflict analysis
 *  - VSIDS-like variable activity
 *  - Random restarts
 *  - **Random polarity** decisions / ADD PHAASE SAVING; do not do random polarity; set to 0 instead instead of random
 *  - Final model printing
 *
 *  To compile:
 *     cc -O2 -o cdcl_random cdcl_random.c
 *  To run:
 *     ./cdcl_random < formula.dimacs
 *
 *  Expect "SAT" or "UNSAT" with an assignment.
 *
 *  The random picks should help avoid incorrectly concluding UNSAT
 *  on tricky crafted formulas like aim-50.dimacs that are truly SAT.
 ************************************************************/

#include <stdio.h>   // I/O (e.g., printf, scanf)
#include <stdlib.h>  // malloc, free, realloc
#include <string.h>  // memcpy, strcpy, etc.
#include <assert.h>  // assert for runtime checks
#include <time.h>    // time for srand()

/************ DEBUG TOGGLES ************/
// These macros can control debug prints if you set them to 1.
// Currently all set to 0 => no debug output
#define DEBUG_MAIN   0
#define DEBUG_WATCH  0
#define DEBUG_PROP   0
#define DEBUG_CONFLICT 0
#define DEBUG_ANALYZE 0

/****************************************/
/* Some basic macros / structures       */
/****************************************/

// val_t enumerates possible variable states:
//  VAL_UNASSIGNED => -1, VAL_FALSE => 0, VAL_TRUE => 1
typedef enum { VAL_UNASSIGNED=-1, VAL_FALSE=0, VAL_TRUE=1 } val_t;

// Each clause holds:
//  - size: number of literals
//  - lits: array of integer literals (positive => var, negative => -var)
//  - learnt: flag if it's a learned (conflict) clause or original
//  - watch0, watch1: indices (in .lits) of the two watched literals
typedef struct {
    int    size;
    int   *lits;
    int    learnt;
    int    watch0;
    int    watch1;
} clause_t;

// For each variable (1..N_VARS), we store:
//  - value: current assignment (VAL_UNASSIGNED, VAL_TRUE, VAL_FALSE)
//  - level: decision level at which it was assigned
//  - reason: pointer to the clause that implied this assignment (NULL if decided)
typedef struct {
    val_t       value;
    int         level;
    clause_t   *reason;
} varinfo_t;

/* We'll keep watchers in an array watchlist_t for each literal index,
   referencing which clauses currently watch that literal. */
typedef struct {
    clause_t **data; // array of pointers to clauses
    int        cap;  // capacity of 'data'
    int        size; // how many watchers are currently stored
} watchlist_t;

/* We'll keep a global "trail" array of variables in the order they
   were assigned. Also an array "trail_lim" to mark the index in the
   trail where each decision level starts. */

// trail[]: sequence of variable IDs in the order they got assigned
static int *trail    = NULL;
static int  trail_sz = 0;

// trail_lim[]: for each decision level dl, trail_lim[dl] = index in trail[] where that level began
static int *trail_lim= NULL; 
static int  trail_lim_sz=0;   // Not explicitly used to limit capacity, but for clarity

// current_dl: the solver's current decision level
static int    current_dl = 0;

// conflict_ct: how many conflicts since last restart
// restart_ct: how many restarts so far
static int    conflict_ct = 0;
static int    restart_ct  = 0;

// RESTART_INTERVAL: initial threshold of conflicts before forcing a restart
static int    RESTART_INTERVAL = 50; // grows over time

/***************************************************/
/* GLOBALS from the input and data structures      */
/***************************************************/

// N_VARS: # of variables, N_CLAUSES: # of original clauses
static unsigned N_VARS=0, N_CLAUSES=0;

// CLAUSES points to all clauses, original + learned
static clause_t *CLAUSES=NULL;
static unsigned CLAUSE_CAP=0;   // capacity
static unsigned TOT_CLAUSES=0;  // total used (original + learned)

// VARINFO: var -> {value, level, reason}
static varinfo_t   *VARINFO= NULL;

// WATCHES: watch for each literal index => watchlist_t
//   array size: (N_VARS+1)*2 because each var can be pos or neg literal
static watchlist_t *WATCHES= NULL;

// We store variable "activity" for a VSIDS-like heuristic
static double      *activity=NULL;
static double       var_inc=1.0;   // how much we increment activity
static double       var_decay=0.8; // how quickly we decay old activity

/* The following structures are used for a potential "order heap"
   approach, though it's not fully utilized here.  */
static int         *order_heap=NULL; 
static int          heap_size=0;
static int         *heap_index=NULL; // var -> index in the heap

// propQ: index into trail for BFS-like propagation, i.e. variables that
// have been assigned but not yet processed for clause updates
static int propQ=0;

/***************************************************/
/* LITERAL <-> INDEX, sign, var_of                */
/***************************************************/
// var_of(lit): returns the variable ID ignoring sign
static inline int var_of(int lit){
    return (lit>0)? lit : -lit;
}
// sign_of(lit): returns 0 if literal is positive, 1 if negative
static inline int sign_of(int lit){
    return (lit>0)? 0 : 1;
}
// lit_index(lit): maps a literal to an index for watchers
//   if lit>0 => index = 2*v
//   if lit<0 => index = 2*v + 1
static inline int lit_index(int lit){
    int v = (lit>0)? lit : -lit;
    return (v<<1) | (lit<0);
}
// index_var(idx): returns variable ID from literal index
// index_sign(idx): returns sign bit from literal index
static inline int index_var(int idx){
    return (idx>>1);
}
static inline int index_sign(int idx){
    return (idx & 1);
}
// index_to_lit(idx): invert lit_index => literal as integer
static inline int index_to_lit(int idx){
    int v= (idx>>1);
    int s= (idx & 1);
    return (s? -v : v);
}
// negate_lit(lit): flips sign
static inline int negate_lit(int lit){
    return -lit;
}

/***************************************************/
/* WATCHLIST GROWTH                                */
/***************************************************/
// watchlist_push(idx, c): add clause c to watchlist for literal index idx
static void watchlist_push(int idx, clause_t* c){
    watchlist_t *wl= &WATCHES[idx];
    if(wl->size==wl->cap){
        wl->cap= (wl->cap<4)?8:(wl->cap*2); // grow capacity
        wl->data= realloc(wl->data, sizeof(clause_t*) * wl->cap);
    }
    wl->data[wl->size++]= c;
}

/***************************************************/
/* VALUE_OF lit, set literal, etc.                 */
/***************************************************/
// value_lit(lit): returns the assigned value of lit => VAL_TRUE/FALSE/UNASSIGNED
static inline val_t value_lit(int lit){
    int v= var_of(lit);         // base variable
    val_t val= VARINFO[v].value; 
    if(val==VAL_UNASSIGNED) return VAL_UNASSIGNED; // not assigned
    int isNeg= (lit<0);
    // if val=TRUE but literal is neg => literal is effectively FALSE, etc.
    if(val==VAL_TRUE)  return (isNeg?VAL_FALSE:VAL_TRUE);
    if(val==VAL_FALSE) return (isNeg?VAL_TRUE:VAL_FALSE);
    return VAL_UNASSIGNED; 
}

// var_bump_activity(v): increases activity[v] by var_inc, with possible rescaling
static inline void var_bump_activity(int v){
    activity[v]+= var_inc;
    // If activity too large, scale everything down
    if(activity[v]>1e100){
        for(unsigned i=1;i<=N_VARS;i++){
            activity[i]*=1e-100;
        }
        var_inc*=1e-100;
    }
}

// var_decay_activity(): multiplies var_inc by 1/var_decay => effectively "var_inc /= var_decay"
static inline void var_decay_activity(void){
    var_inc*= (1.0/var_decay);
}

// enqueue(lit, reason): assign variable for lit => TRUE/FALSE based on sign,
// set reason if implied, push var onto trail. If conflict, return 0.
static int enqueue(int lit, clause_t* reason){
    int v= var_of(lit);
    val_t val= VARINFO[v].value;
    if(val != VAL_UNASSIGNED){
        // Already assigned => check for conflict
        if(value_lit(lit)==VAL_FALSE){
            return 0; // conflict
        }
        return 1; // consistent with existing assignment
    }
    // Perform assignment
    VARINFO[v].value= (lit>0 ? VAL_TRUE : VAL_FALSE);
    VARINFO[v].level= current_dl;
    VARINFO[v].reason= reason;
    // Add var to the assignment trail
    trail[trail_sz++]= v;
    return 1;
}

/***************************************************/
/* CANCEL (backtrack)                              */
/***************************************************/
// cancel_until(level): unassign everything above 'level'
static void cancel_until(int level){
    // while our current decision level > requested level
    while(current_dl> level){
        // unassign all variables assigned at that decision level
        for(int c= trail_lim[current_dl]; c< trail_sz; c++){
            int v= trail[c];
            VARINFO[v].value= VAL_UNASSIGNED;
            VARINFO[v].reason=NULL;
        }
        // set trail_sz back to where that level began
        trail_sz= trail_lim[current_dl];
        current_dl--;
    }
    // fix propQ if needed
    if(propQ> trail_sz) propQ= trail_sz;
}

/***************************************************/
/* ORDER HEAP                                      */
/***************************************************/
// The code below is for a priority-heap approach to variable picking,
// but the solver mostly uses a custom approach (decideVar). 
// Some solvers maintain a binary heap of variables sorted by activity.

/* heap_swap: swap items in the order_heap, fix their indices */
static inline void heap_swap(int i,int j){
    int va= order_heap[i];
    int vb= order_heap[j];
    order_heap[i]= vb;
    order_heap[j]= va;
    heap_index[vb]= i;
    heap_index[va]= j;
}
static void heap_sift_up(int i){
    int v= order_heap[i];
    double act= activity[v];
    while(i>0){
        int p=(i-1)>>1;
        int vp= order_heap[p];
        if(activity[vp]>= act) break;
        heap_swap(p,i);
        i=p;
    }
}
static void heap_sift_down(int i){
    while(1){
        int left =(i<<1)+1;
        int right=(i<<1)+2;
        int largest=i;
        if(left<heap_size){
            int lv= order_heap[left];
            if(activity[lv]> activity[ order_heap[largest]]) largest= left;
        }
        if(right<heap_size){
            int rv= order_heap[right];
            if(activity[rv]> activity[ order_heap[largest]]) largest= right;
        }
        if(largest==i) break;
        heap_swap(i,largest);
        i= largest;
    }
}
static void heap_insert_var(int v){
    if(heap_index[v]>=0) return; 
    int i= heap_size++;
    order_heap[i]= v;
    heap_index[v]= i;
    heap_sift_up(i);
}
static int heap_pop(void){
    if(!heap_size) return 0;
    int v= order_heap[0];
    heap_size--;
    order_heap[0]= order_heap[heap_size];
    heap_index[ order_heap[0]] = 0;
    heap_index[v]= -1;
    heap_sift_down(0);
    return v;
}

/***************************************************/
/* PROPAGATION (two-literal watch)                */
/***************************************************/
// propagate(): process newly assigned variables from the trail
// for each such var v, we look at watchers of the opposite literal -lit
// and see if the clause can still be satisfied or is conflicting
static clause_t* propagate(void){
    // Keep going while there are unprocessed assignments in trail
    while(propQ< trail_sz){
        int v= trail[propQ++];         // variable assigned
        val_t val= VARINFO[v].value;   // was it assigned true or false?
        // reconstruct the literal from var + sign
        int lit= (val==VAL_TRUE)? v : -v;

        // For watchers of -lit, we see if the clause is now in trouble
        int idx= lit_index(-lit);
        watchlist_t *wl= &WATCHES[idx];
        int j=0; // pointer to keep valid watchers in wl->data

        // Iterate through watchers
        for(int i=0; i< wl->size; i++){
            clause_t* c= wl->data[i];
            int w0= c->watch0;      // index in c->lits
            int w1= c->watch1;
            int L0= c->lits[w0];    // actual literal
            int L1= c->lits[w1];

            // If the first watched literal == -lit, we swap them
            if(L0== -lit){
                c->watch0= w1; 
                c->watch1= w0;
                L0= c->lits[c->watch0];
                L1= c->lits[c->watch1];
            }
            // if L0 is already satisfied => no problem
            if(value_lit(L0)==VAL_TRUE){
                wl->data[j++]= c; // keep in the watch list
                continue;
            }
            // Otherwise, attempt to find a new literal in c->lits to watch
            // that isn't false
            int found=0;
            for(int k=0; k< c->size; k++){
                if(k== c->watch0 || k== c->watch1) continue;
                int cand= c->lits[k];
                if(value_lit(cand)!=VAL_FALSE){
                    // watch this new literal
                    c->watch1= k;
                    watchlist_push( lit_index(cand), c);
                    found=1;
                    break;
                }
            }
            if(found){
                // We've successfully moved the watch, so remove c
                // from the old watch list
                continue;
            }

            // If we couldn't find a new watch => check conflict or unit
            if(value_lit(L0)==VAL_FALSE){
                // conflict => entire clause is false
                wl->data[j++]= c; // keep it for now
                return c;         // return pointer to conflict clause
            }
            // If L0 is unassigned => we have a forced assignment
            if(value_lit(L0)==VAL_UNASSIGNED){
                if(!enqueue(L0, c)){
                    // conflict if enqueue fails
                    wl->data[j++]= c;
                    return c;
                }
            }
            wl->data[j++]= c;
        }
        // wl->size shrinks to j => removing watchers we moved away
        wl->size= j;
    }
    return NULL; // no conflict
}

/***************************************************/
/* 1-UIP CONFLICT ANALYSIS                         */
/***************************************************/
// We'll store 'seen' flags, an analysis stack, etc. to handle the backtrace

static char    *seen=NULL;     // var -> 1 if visited in analysis, else 0
static int     *an_stack=NULL; // stack for unmarking
static int      an_stack_sz=0; // size of an_stack in use
static int     *learnt_arr=NULL; // store the new conflict clause's literals
static int      learnt_sz=0;    // how many in learnt_arr

// unmark_all(): resets 'seen[v]' for all variables v we visited
static void unmark_all(void){
    while(an_stack_sz>0){
        int v= an_stack[--an_stack_sz];
        seen[v]=0;
    }
}

// analyze(confl, out_btlevel): do 1-UIP analysis on the conflict
//  confl: pointer to the conflict clause
//  out_btlevel: the backtrack level to which we will jump
// returns a pointer to the newly learned clause
static clause_t* analyze(clause_t* confl, int* out_btlevel){
    learnt_sz=0; // no literals in new clause yet
    int pathC=0; // how many vars in current conflict level
    int p=-1;    // the last assigned var in the conflict graph
    clause_t* reason = confl;
    int idx= trail_sz-1; // start from top of trail

    do {
        // For each literal q in the reason clause
        for(int i=0;i< reason->size;i++){
            int q= reason->lits[i];
            int v= var_of(q);
            if(!seen[v]){
                seen[v]=1; // mark visited
                an_stack[an_stack_sz++]= v;
                // if that var is from the current level => pathC++
                if(VARINFO[v].level== current_dl) pathC++;
                else learnt_arr[learnt_sz++]= q; // otherwise, it's part of the new clause
            }
        }
        // Move idx down the trail until we find a var that is 'seen'
        while(!seen[ trail[idx] ]) idx--;
        p= trail[idx];
        idx--;
        reason= VARINFO[p].reason; // the clause that implied p
        pathC--;
    } while(pathC>0);

    // p is the first UIP (Unique Implication Point)
    // we add the negation of p's assignment to the learned clause
    learnt_arr[learnt_sz++]= (VARINFO[p].value==VAL_TRUE)? -p : p;

    // find second highest level among the other learned-literals
    int backL=0;
    for(int i=0;i<learnt_sz-1;i++){
        int v= var_of(learnt_arr[i]);
        int lvl= VARINFO[v].level;
        if(lvl> backL && lvl< current_dl){
            backL= lvl;
        }
    }
    *out_btlevel= backL;

    // build the new learned clause
    clause_t* newc= (clause_t*) malloc(sizeof(clause_t));
    newc->size= learnt_sz;
    newc->lits= (int*) malloc(sizeof(int)* learnt_sz);
    newc->learnt=1; // mark as learned
    for(int i=0;i< learnt_sz;i++){
        newc->lits[i]= learnt_arr[i];
        // bump variable activity => these vars participated in conflict
        var_bump_activity( var_of(learnt_arr[i]) );
    }
    // watch the first two literals
    newc->watch0=0;
    newc->watch1= (learnt_sz>1)?1:0;

    // we store this new clause in CLAUSES, resizing if necessary
    if(TOT_CLAUSES==CLAUSE_CAP){
        CLAUSE_CAP= (CLAUSE_CAP<8)?16:(CLAUSE_CAP*2);
        CLAUSES= (clause_t*) realloc(CLAUSES, CLAUSE_CAP*sizeof(clause_t));
    }
    CLAUSES[TOT_CLAUSES]= *newc;
    clause_t* ret=&CLAUSES[TOT_CLAUSES];
    TOT_CLAUSES++;
    free(newc);

    // watch the newly learned clause's two watchers
    watchlist_push( lit_index(ret->lits[ ret->watch0 ]), ret);
    watchlist_push( lit_index(ret->lits[ ret->watch1 ]), ret);

    // unmark all visited variables
    unmark_all();
    return ret;
}

/***************************************************/
/* DECIDE: random polarity                         */
/***************************************************/
// decideVar(): pick an unassigned var with highest activity
// if none found, fallback to the first unassigned
static int decideVar(void){
    int bestVar=0;
    double bestAct=-1.0;
    // linear search for max activity among unassigned variables
    for(unsigned i=1; i<=N_VARS; i++){
        if(VARINFO[i].value==VAL_UNASSIGNED){
            if(activity[i]> bestAct){
                bestAct= activity[i];
                bestVar=i;
            }
        }
    }
    // if we found none with an activity improvement, fallback
    if(!bestVar){
        for(unsigned i=1;i<=N_VARS;i++){
            if(VARINFO[i].value==VAL_UNASSIGNED){
                bestVar=i;
                break;
            }
        }
    }
    return bestVar;
}

// pickPolarityRandom(var): returns var or -var with 50/50 chance
static int pickPolarityRandom(int var){
    int r= rand()%2;
    return (r? var : -var);
}

/***************************************************/
/* MAIN SEARCH                                     */
/***************************************************/
// search_inner(): main DPLL/CDCL loop
//  - do propagate()
//  - if conflict => analyze + backjump
//  - else check if all assigned => SAT
//  - else decide a variable with random polarity
//  - random restarts if conflict count > RESTART_INTERVAL
static int search_inner(void){
    while(1){
        clause_t* confl= propagate();
        if(confl){
            // conflict
            conflict_ct++;
            if(current_dl==0){
                // conflict at level0 => unsat
                return 0;
            }
            int btlevel=0;
            clause_t* c= analyze(confl,&btlevel);
            // backjump
            cancel_until(btlevel);
            // enqueue the first literal of the learned clause at that level
            if(!enqueue( c->lits[0], c)){
                // if that fails => unsat
                return 0;
            }
        } else {
            // no conflict => check if all variables assigned => SAT
            int assigned=1;
            for(unsigned i=1; i<=N_VARS; i++){
                if(VARINFO[i].value==VAL_UNASSIGNED){
                    assigned=0;
                    break;
                }
            }
            if(assigned) return 1; // sat

            // not all assigned => new decision
            current_dl++;
            trail_lim[current_dl]= trail_sz;

            int var= decideVar();
            if(!var) return 1; // if no var found => all assigned => sat
            int pLit= pickPolarityRandom(var);
            // assign new variable
            if(!enqueue(pLit, NULL)){
                return 0;
            }
        }
        // implement random restarts
        if(conflict_ct >= RESTART_INTERVAL){
            conflict_ct=0;
            // grow the interval
            RESTART_INTERVAL= (RESTART_INTERVAL*3)/2 + 10;
            // backtrack to level0
            cancel_until(0);
        }
    }
}

/***************************************************/
/* MAIN                                            */
/***************************************************/
int main(int argc, char** argv){
    // seed random for random polarity
    srand(time(NULL));

    // skip lines starting with 'c'
    for(char c;(c=getc(stdin))=='c'; ){
        while(getc(stdin)!='\n');
    }

    // read "cnf <N_VARS> <N_CLAUSES>" line
    assert(scanf(" cnf %u %u\n", &N_VARS,&N_CLAUSES)==2);

    // allocate space for clauses
    CLAUSE_CAP= (N_CLAUSES+16);
    CLAUSES= (clause_t*) calloc(CLAUSE_CAP,sizeof(clause_t));
    TOT_CLAUSES= N_CLAUSES; // initially only the original ones

    // allocate varinfo array
    VARINFO= (varinfo_t*) calloc(N_VARS+1,sizeof(varinfo_t));
    for(unsigned i=1;i<=N_VARS;i++){
        VARINFO[i].value= VAL_UNASSIGNED; // none assigned
        VARINFO[i].level=0;
        VARINFO[i].reason=NULL;
    }

    // allocate watchers array => 2 for each var (pos+neg), plus one for var=0
    WATCHES= (watchlist_t*) calloc((N_VARS+1)*2,sizeof(watchlist_t));
    for(unsigned i=0;i<(N_VARS+1)*2;i++){
        WATCHES[i].cap=0;
        WATCHES[i].size=0;
        WATCHES[i].data=NULL;
    }

    // activity array => used by decideVar
    activity= (double*) calloc(N_VARS+1,sizeof(double));
    for(unsigned i=1;i<=N_VARS;i++){
        activity[i]=0.0;
    }

    // read each clause from input
    for(unsigned i=0;i<N_CLAUSES;i++){
        int arrCap=4, arrSz=0;
        int *arr = (int*) malloc(sizeof(int)*arrCap);
        int lit=0;
        // read literals until 0 is encountered
        while(1){
            assert(scanf("%d",&lit)==1);
            if(!lit) break; // end of clause
            if(arrSz==arrCap){
                arrCap*=2;
                arr= (int*) realloc(arr, sizeof(int)*arrCap);
            }
            arr[arrSz++]= lit;
        }
        // store them in CLAUSES[i]
        CLAUSES[i].size= arrSz;
        CLAUSES[i].lits= arr;
        CLAUSES[i].learnt=0;     // original clause => not learnt
        // watch the first two literals if possible
        CLAUSES[i].watch0=0;
        CLAUSES[i].watch1=(arrSz>1)?1:0;
    }

    // initialize watchers => push each clause into watchers of its watch0, watch1
    for(unsigned i=0;i<N_CLAUSES;i++){
        int L0= CLAUSES[i].lits[ CLAUSES[i].watch0 ];
        int L1= CLAUSES[i].lits[ CLAUSES[i].watch1 ];
        watchlist_push( lit_index(L0), &CLAUSES[i]);
        watchlist_push( lit_index(L1), &CLAUSES[i]);
    }

    // build the trail array
    trail= (int*) malloc(sizeof(int)*(N_VARS+1)*2);
    trail_sz=0;
    trail_lim= (int*) malloc(sizeof(int)*(N_VARS+1));
    trail_lim[0]=0;      // level0 starts at index 0
    current_dl=0;

    // conflict analysis structures
    seen= (char*) calloc(N_VARS+1,sizeof(char));       // seen[v] => visited in analysis
    an_stack= (int*) malloc(sizeof(int)*(N_VARS+1));   // stack for unmarking
    learnt_arr= (int*) malloc(sizeof(int)*(N_VARS+1)*2); // store new clause

    // Launch the CDCL search
    int ret= search_inner();
    if(!ret){
        // unsatisfiable
        printf("UNSAT\n");
        return 0;
    } 
    // if ret != 0 => SAT. print assignment
    printf("SAT\n");
    for(unsigned v=1; v<=N_VARS; v++){
        if(VARINFO[v].value==VAL_TRUE) printf("%u ",v);
        else printf("-%u ",v);
    }
    printf("\n");
    return 0;
}
