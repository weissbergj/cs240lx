// basic-comments // single-line comment indicating section/purpose

#include <stdio.h>    // printf, scanf, etc.
#include <stdlib.h>   // malloc, free, realloc
#include <string.h>   // memcpy, strlen, etc.
#include <assert.h>   // assert for debugging; halts if condition fails
#include <time.h>     // time, srand, rand

// *****************************************************************************
// *          The `append_field` macro is used to dynamically grow arrays       *
// *****************************************************************************
#define append_field(OBJ, FIELD) (*({ \ // macro to add elements to OBJ.FIELD via GNU extension
    (OBJ).FIELD = realloc((OBJ).FIELD, (++((OBJ).n_##FIELD)) * sizeof((OBJ).FIELD[0])); \
        /* ^ reallocate OBJ.FIELD, increment n_FIELD, expand by one element  */ \
    (OBJ).FIELD + ((OBJ).n_##FIELD - 1); \ // returns pointer to the new (last) slot
}))

// *****************************************************************************
// *                              GLOBALS                                      *
// *****************************************************************************
// N_VARS, N_CLAUSES: total number of variables, total number of clauses
static unsigned N_VARS=0, N_CLAUSES=0; 

// -----------------------------------------------------------------------------
// Clause structure: each clause has an array of literal ints, the count of those
// literals, and a count of how many of them are currently false (n_zeros).
// -----------------------------------------------------------------------------
struct clause {
    int *lits;   // pointer to an array of integer literals
    int n_lits;  // how many literals in this clause
    int n_zeros; // how many of those are currently evaluated as 'false'
};
static struct clause *CLAUSES=NULL;           // dynamic array of all clauses
static unsigned TOTAL_CLAUSES=0, MAX_CLAUSES=0; // tracking how many are in use, capacity

// -----------------------------------------------------------------------------
// We represent variable assignments with an enum: unassigned, false, or true
// -----------------------------------------------------------------------------
enum assignment { UNASSIGNED=-1, FALSE=0, TRUE=1 };
static enum assignment *ASSIGNMENT=NULL; // array of assignments for each variable index

// -----------------------------------------------------------------------------
// A 'decision' means we assigned a variable either by an implication (BCP) or
// by a top-level guess. We track these in a stack to allow backtracking.
// -----------------------------------------------------------------------------
enum decision_type { IMPLIED=0, TRIED_ONE_WAY=1, TRIED_BOTH_WAYS=2 };
struct decision {
    unsigned var;           // which variable was assigned
    enum decision_type type; // how it was assigned (implied vs. guess, etc.)
};
static struct decision *DECISION_STACK=NULL; // dynamic stack of decisions
static unsigned N_DECISION_STACK=0;          // how many decisions are on the stack

// -----------------------------------------------------------------------------
// Each literal can be "watched" or "touched" by multiple clauses. The clause_list
// is a dynamic array of references to clauses that contain a specific literal.
// -----------------------------------------------------------------------------
struct clause_list {
    struct clause **clauses; // array of pointers to clauses
    int n_clauses;           // how many pointers currently stored
};
static struct clause_list *LIT_TO_CLAUSES=NULL; // one list per literal ID

// *****************************************************************************
// *                              Helper Functions                              *
// *****************************************************************************

// Return the absolute value of an integer x
static inline int my_abs(int x){ 
    return (x < 0) ? -x : x; 
}

// Convert literal L to an ID: we map variable "v" to 2*v or 2*v+1 depending on sign
//    For example, if L > 0, lit_id(L) = 2*v + 1
//                 if L < 0, lit_id(L) = 2*v + 0
static inline int lit_id(int L){ 
    return 2*my_abs(L) + (L > 0); 
}

// Return a pointer to the clause_list tracking all clauses that contain literal L
static inline struct clause_list*clauses_touching(int L){ 
    return &LIT_TO_CLAUSES[ lit_id(L) ]; 
}

// Returns 1 if literal L is true under the current assignment, else 0
static inline int is_lit_true(int L){
    int v = my_abs(L);          // underlying variable index
    if(ASSIGNMENT[v] == UNASSIGNED) 
        return 0;               // can't be true if unassigned
    return (ASSIGNMENT[v] == (L > 0)); 
}

// *****************************************************************************
// *                           Print final results                              *
// *****************************************************************************

// If we found a satisfying assignment, print "SAT" and each variable's sign
static int satisfiable(void){
    printf("SAT\n");
    // we skip index 0 because we stored variables from 1..(N_VARS-1)
    for(unsigned i = 1; i < N_VARS; i++){
        if(ASSIGNMENT[i] == TRUE)  printf("%u ", i);
        else                       printf("-%u ", i);
    }
    printf("\n");
    return 0; // typical exit code for 'found solution'
}

// If unsatisfiable, just print "UNSAT"
static int unsatisfiable(void){
    printf("UNSAT\n");
    return 0;
}

// *****************************************************************************
// *                             set / unset                                    *
// *                    Functions to assign or revert assignment                *
// *****************************************************************************

// set_literal(L, t): tries to assign variable (|L|) to TRUE if L>0 or FALSE if L<0.
// If there's already a conflicting assignment, we return 0 (conflict).
// Otherwise, we update the decision stack and increment the "false count" in each
// clause that contains the opposite literal.
static int set_literal(int L, enum decision_type t){
    int v = my_abs(L); // variable index
    if(ASSIGNMENT[v] != UNASSIGNED){
        // It's already assigned. Check for conflict.
        int assigned = (ASSIGNMENT[v] == TRUE) ? 1 : 0; // 1 if it was TRUE
        int want     = (L > 0) ? 1 : 0;                 // 1 if we want TRUE
        if(assigned != want){
            // conflict
            return 0; 
        }
        // no conflict if same assignment
        return 1;
    }
    // Actually set the assignment
    ASSIGNMENT[v] = (L > 0) ? TRUE : FALSE;

    // Record it in the decision stack
    DECISION_STACK[N_DECISION_STACK].var  = v;
    DECISION_STACK[N_DECISION_STACK].type = t;
    N_DECISION_STACK++;

    // For clauses containing the opposite literal, we increase their n_zeros
    // because we just made that opposite literal false.
    int opp = -L;
    struct clause_list *cl = clauses_touching(opp);
    for(int i = 0; i < cl->n_clauses; i++){
        struct clause *C = cl->clauses[i];
        C->n_zeros++;
        // If all literals in C are now false, that is a direct conflict
        if(C->n_zeros == C->n_lits){
            return 0; 
        }
    }
    return 1;
}

// unset_latest_assignment(): Revert the last assignment decision. This is used
// during backtracking or flipping a variable. We also decrease n_zeros in the
// clauses that were impacted by that assignment.
static void unset_latest_assignment(void){
    // Pop the top decision
    N_DECISION_STACK--;
    unsigned v = DECISION_STACK[N_DECISION_STACK].var;
    // We figure out which literal was set by checking if it was assigned TRUE or FALSE
    int L = (ASSIGNMENT[v] == TRUE) ? (int)v : -(int)v;

    // Mark that variable as unassigned
    ASSIGNMENT[v] = UNASSIGNED;

    // For all clauses containing the opposite of that literal, we decrease n_zeros
    int opp = -L;
    struct clause_list *cw = clauses_touching(opp);
    for(int i = 0; i < cw->n_clauses; i++){
        cw->clauses[i]->n_zeros--;
    }
}

// *****************************************************************************
// *                          Append clause                                     *
// *        Build or learn new clauses in the global array, update watchers     *
// *****************************************************************************

// append_clause(arr, n): creates a new clause containing n literals (arr[0..n-1]).
static void append_clause(int *arr, int n){
    // Ensure there's enough room in CLAUSES array
    if(TOTAL_CLAUSES == MAX_CLAUSES){
        MAX_CLAUSES = (MAX_CLAUSES < 8) ? 8 : (MAX_CLAUSES*2);
        CLAUSES = realloc(CLAUSES, MAX_CLAUSES * sizeof(struct clause));
    }

    // Grab a pointer to the new clause and increment count
    struct clause *C = &CLAUSES[TOTAL_CLAUSES++];
    // Initialize fields
    C->lits = NULL;
    C->n_lits = 0;
    C->n_zeros = 0;

    // For each literal in arr, append it to C->lits
    for(int i = 0; i < n; i++){
        append_field((*C), lits) = arr[i];
    }

    // Update watchers: each literal in this new clause should list "C" in its
    // LIT_TO_CLAUSES table
    for(int i = 0; i < n; i++){
        struct clause_list *cl = clauses_touching(arr[i]);
        append_field((*cl), clauses) = C;
    }

    // Initialize n_zeros: how many of these literals are currently false?
    int z = 0;
    for(int i = 0; i < n; i++){
        int L = arr[i];
        int v = my_abs(L);
        if(ASSIGNMENT[v] != UNASSIGNED){
            // A literal is false if it's positive and assigned FALSE,
            // or if it's negative and assigned TRUE
            int assignedFalse = ((L > 0 && ASSIGNMENT[v] == FALSE) ||
                                 (L < 0 && ASSIGNMENT[v] == TRUE));
            if(assignedFalse) z++;
        }
    }
    // Store final counts
    C->n_lits  = n;
    C->n_zeros = z;
}

// *****************************************************************************
// *                           BCP - Unit Propagation                           *
// *****************************************************************************

// bcp(): repeatedly scans all clauses. If a clause is "all but one literal false",
// we must force the remaining unassigned literal to be TRUE. If a conflict arises,
// return 0; otherwise 1 if no conflict.
static int bcp(void){
    int changed = 1; 
    while(changed){
        changed = 0;
        // Scan every clause
        for(unsigned i = 0; i < TOTAL_CLAUSES; i++){
            struct clause *C = &CLAUSES[i];
            // If (n_zeros + 1) == n_lits => exactly one literal can be true
            if(C->n_zeros + 1 == C->n_lits){
                // Check if the clause is already satisfied by some true literal
                int sat = 0;
                for(int j = 0; j < C->n_lits; j++){
                    if(is_lit_true(C->lits[j])){ 
                        sat = 1; 
                        break; 
                    }
                }
                if(sat) 
                    continue; // no need to force anything if it's already satisfied

                // We must force the leftover unassigned literal to be true
                for(int j = 0; j < C->n_lits; j++){
                    int L = C->lits[j];
                    int v = my_abs(L);
                    if(ASSIGNMENT[v] == UNASSIGNED){ 
                        // Try setting it. If conflict => bcp fails
                        if(!set_literal(L, IMPLIED)){
                            return 0;
                        }
                        changed = 1; 
                        break; 
                    }
                }
            }
        }
    }
    // No conflicts found
    return 1;
}

// *****************************************************************************
// *                        Conflict Clause Recording                           *
// *****************************************************************************
/*
If a conflict arises, we learn a new clause that "explains" the conflict:
  - We look at the most recent decision block (the last TRIED_ONE_WAY).
  - The new clause comprises the negation of all assignments made since that decision.
  - If no TRIED_ONE_WAY is found but there's still a partial stack,
    we gather the entire decision stack, meaning we jump even further back.
  - If the stack is empty, we have a top-level conflict => formula is unsatisfiable.
*/
static void record_conflict_clause(void){
    // find last TRIED_ONE_WAY
    int idx = N_DECISION_STACK - 1;
    while(idx >= 0 && DECISION_STACK[idx].type != TRIED_ONE_WAY){
        idx--;
    }

    // If none found
    if(idx < 0){
        // If decision stack is empty => conflict at level0 => unsat => skip learning
        if(N_DECISION_STACK == 0){
            return; 
        }
        // Otherwise, gather the entire stack => deeper backjump
        int count = N_DECISION_STACK;
        int *arr = malloc(count * sizeof(int));
        int w = 0;
        for(int s = 0; s < (int)N_DECISION_STACK; s++){
            unsigned v = DECISION_STACK[s].var;
            // If a variable is assigned TRUE, record -v (meaning "not v")
            // If assigned FALSE, record +v
            int sign = (ASSIGNMENT[v] == TRUE)? -1 : 1;
            arr[w++] = sign * (int)v;
        }
        append_clause(arr, w);
        free(arr);
        return;
    }

    // Otherwise, gather from idx+1..end
    int firstImplied = idx + 1;
    if(firstImplied >= (int)N_DECISION_STACK){
        // No implied decisions => single-literal flipping the top-level decision
        unsigned v = DECISION_STACK[idx].var;
        int sign = (ASSIGNMENT[v] == TRUE)? -1 : 1;
        int arr[1];
        arr[0] = sign*(int)v;
        append_clause(arr,1);
        return;
    }

    // Gather top-level block
    int count = N_DECISION_STACK - firstImplied;
    int *arr = malloc(count * sizeof(int));
    int w = 0;
    for(int s = firstImplied; s < (int)N_DECISION_STACK; s++){
        unsigned v = DECISION_STACK[s].var;
        int sign = (ASSIGNMENT[v] == TRUE)? -1 : 1;
        arr[w++] = sign * (int)v;
    }
    append_clause(arr, w);
    free(arr);
}

// *****************************************************************************
// *                         Resolve Conflict (Backjump)                        *
// *****************************************************************************

// resolveConflict(): after a conflict, we record a new conflict clause, then try
// to backtrack to a prior decision level. We pop implied decisions until we hit
// TRIED_ONE_WAY. Then we flip that assignment from false->true or true->false.
// If that flip leads to an immediate conflict, we keep backtracking. 
static int resolveConflict(void){
    // record new clause from the conflict
    record_conflict_clause();

    while(1){
        // pop implied decisions
        while(N_DECISION_STACK > 0 &&
              DECISION_STACK[N_DECISION_STACK-1].type != TRIED_ONE_WAY){
            unset_latest_assignment();
        }
        // If no top-level decisions left => unsat
        if(!N_DECISION_STACK){
            return 0;
        }
        // Flip the top-level decision
        unsigned v = DECISION_STACK[N_DECISION_STACK - 1].var;
        unset_latest_assignment(); // unassign it

        // If it was false => now make it true, etc.
        // (We see the old assignment from ASSIGNMENT[v], but we just unassigned it.)
        // So let's define newVal: if it was TRUE => newVal=0 => we set it false; etc.
        int newVal = (ASSIGNMENT[v] == TRUE)? 0 : 1; 
        int L = (newVal) ? (int)v : -(int)v;

        // TRIED_BOTH_WAYS indicates we've flipped this variable from its original guess
        if(!set_literal(L, TRIED_BOTH_WAYS)){
            // immediate conflict => loop again, continue backtracking
            continue;
        }
        // Once we flip, we run BCP again
        while(!bcp()){
            record_conflict_clause();
            // If conflict arises immediately again, jump back up
            goto conflict_again;
        }
        return 1; 
    conflict_again:;
    }
}

// *****************************************************************************
// *                  Decide a new variable assignment (heuristic)             *
// *****************************************************************************

// decide(): pick a random unassigned variable and assign it false (TRIED_ONE_WAY).
// returns 0 if no unassigned variables remain (we might be done).
static int decide(void){
    // gather unassigned variables into an array
    int *u = malloc(sizeof(int)*(N_VARS - 1));
    int nu = 0;
    for(unsigned i = 1; i < N_VARS; i++){
        if(ASSIGNMENT[i] == UNASSIGNED){
            u[nu++] = i;
        }
    }
    if(!nu){
        free(u);
        return 0; // no unassigned => done
    }

    // pick a random variable
    int pick = rand() % nu;
    int var  = u[pick];
    free(u);

    // Attempt to assign it FALSE as a top-level guess
    if(!set_literal(-var, TRIED_ONE_WAY)){
        // conflict from that assignment => immediate fail
        return 0; 
    }
    return 1; // success
}

// *****************************************************************************
// *                    Check all clauses for satisfaction                      *
// *****************************************************************************

// checkAllClauses(): returns 1 if every clause has at least one true literal,
// otherwise returns 0
static int checkAllClauses(void){
    for(unsigned i = 0; i < TOTAL_CLAUSES; i++){
        struct clause *C = &CLAUSES[i];
        int sat = 0;
        // If any literal is true, the clause is satisfied
        for(int j = 0; j < C->n_lits; j++){
            if(is_lit_true(C->lits[j])){ 
                sat = 1; 
                break; 
            }
        }
        if(!sat) 
            return 0; // found an unsatisfied clause
    }
    return 1; // all satisfied
}

// *****************************************************************************
// *                                 MAIN                                       *
// *****************************************************************************

int main(int argc, char** argv){
    // Seed the random generator for our "decide" heuristic
    srand(time(NULL));

    // Skip any lines starting with 'c' (comment lines in a typical CNF file)
    for(char c; (c = getc(stdin)) == 'c'; ){
        while(getc(stdin) != '\n'); // read until end of line
    }

    // Now we expect a line like: " cnf <N_VARS> <N_CLAUSES>"
    // Example: "p cnf 3 5" => 3 variables, 5 clauses
    assert(scanf(" cnf %u %u\n", &N_VARS, &N_CLAUSES) == 2);

    // The code uses variables from index 1..N_VARS-1, so we do N_VARS++
    N_VARS++;
    // Prepare space for clauses
    MAX_CLAUSES = N_CLAUSES * 2; 
    CLAUSES = calloc(MAX_CLAUSES, sizeof(struct clause));
    TOTAL_CLAUSES = N_CLAUSES; 

    // Allocate arrays for assignments, decision stack, watchers
    ASSIGNMENT = malloc(N_VARS * sizeof(*ASSIGNMENT));
    for(unsigned i = 0; i < N_VARS; i++){
        ASSIGNMENT[i] = UNASSIGNED;
    }
    DECISION_STACK = calloc(N_VARS, sizeof(struct decision));
    LIT_TO_CLAUSES = calloc(2*N_VARS, sizeof(struct clause_list));

    // *************************************************************************
    // *         Read the original clauses from STDIN (after the CNF line)     *
    // *************************************************************************
    for(unsigned i = 0; i < N_CLAUSES; i++){
        while(1){
            int L;
            // read the next literal integer
            assert(scanf("%d", &L) == 1);
            // if it's 0, we've reached the end of this clause
            if(!L) break;
            // otherwise, append this literal to the i-th clause
            append_field(CLAUSES[i], lits) = L;
        }
        // watchers: for each literal in the i-th clause, add &CLAUSES[i] to that literal's watchers
        for(int j = 0; j < CLAUSES[i].n_lits; j++){
            struct clause_list *cl = clauses_touching(CLAUSES[i].lits[j]);
            append_field((*cl), clauses) = &CLAUSES[i];
        }
        // compute how many of these literals are currently false
        int z = 0;
        for(int j = 0; j < CLAUSES[i].n_lits; j++){
            int LL = CLAUSES[i].lits[j];
            int vv = my_abs(LL);
            if(ASSIGNMENT[vv] != UNASSIGNED){
                int assignedFalse = ((LL > 0 && ASSIGNMENT[vv] == FALSE)
                                     || (LL < 0 && ASSIGNMENT[vv] == TRUE));
                if(assignedFalse) z++;
            }
        }
        CLAUSES[i].n_zeros = z;
    }

    // *************************************************************************
    // *                          Initial Propagation                           *
    // *************************************************************************
    // Immediately run BCP to propagate any unit clauses that might exist
    if(!bcp()){ 
        // If there's already a conflict, record and attempt to resolve
        record_conflict_clause();
        if(!resolveConflict()){
            return unsatisfiable(); // no resolution => formula is UNSAT
        }
    }

    // *************************************************************************
    // *                             Main Solving Loop                          *
    // *************************************************************************
    while(1){
        // Try to pick an unassigned variable to decide (assign it false)
        if(!decide()){
            // If we can't find any unassigned variables => check if solution is valid
            if(checkAllClauses()){
                return satisfiable(); 
            } else {
                // If it's not valid, try to resolve conflict. Possibly there's a missed conflict.
                record_conflict_clause();
                if(!resolveConflict()){
                    return unsatisfiable();
                }
            }
        }
        // After deciding a variable, propagate again. If conflict arises => fix it.
        while(!bcp()){
            record_conflict_clause();
            if(!resolveConflict()){
                return unsatisfiable();
            }
        }
    }
    // We never reach here in normal conditions
    return 0;
}
