// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <assert.h>
// #include <time.h>

// #define append_field(OBJ, FIELD) (*({  \
//     (OBJ).FIELD = realloc((OBJ).FIELD, (++((OBJ).n_##FIELD)) * sizeof((OBJ).FIELD[0])); \
//     (OBJ).FIELD + ((OBJ).n_##FIELD - 1); \
// }))

// /****** DEBUG TOGGLE ******/
// #define DEBUG_PRINT 1
// #define DEBUG_CONFLICT 1

// int LOG_XCHECK=1;
// #define xprintf(...) do { if(LOG_XCHECK){ fprintf(stderr,__VA_ARGS__); } } while(0)

// /****** GLOBALS ******/
// static unsigned N_VARS=0, N_CLAUSES=0;

// struct clause {
//     int *lits;
//     int  n_lits;
//     int  n_zeros;
// };
// static struct clause *CLAUSES=NULL;
// static unsigned TOTAL_CLAUSES=0, MAX_CLAUSES=0;

// enum assignment { UNASSIGNED=-1, FALSE=0, TRUE=1 };
// static enum assignment *ASSIGNMENT=NULL;

// // decisions
// enum decision_type { IMPLIED=0, TRIED_ONE_WAY=1, TRIED_BOTH_WAYS=2 };
// struct decision {
//     unsigned var;
//     enum decision_type type;
// };
// static struct decision *DECISION_STACK=NULL;
// static unsigned N_DECISION_STACK=0;

// struct clause_list {
//     struct clause **clauses;
//     int n_clauses;
// };
// static struct clause_list *LIT_TO_CLAUSES=NULL;

// /****** Helper ******/
// static inline int my_abs(int x){ return (x<0)?-x:x; }
// static inline int lit_id(int L){ return 2*my_abs(L)+(L>0); }
// static inline struct clause_list*clauses_touching(int L){ 
//     return &LIT_TO_CLAUSES[ lit_id(L) ]; 
// }
// static inline int is_lit_true(int L){
//     int v=my_abs(L);
//     if(ASSIGNMENT[v]==UNASSIGNED) return 0;
//     return (ASSIGNMENT[v]==(L>0));
// }

// /****** Print final ******/
// static int satisfiable(void){
//     printf("SAT\n");
//     for(unsigned i=1;i<N_VARS;i++){
//         if(ASSIGNMENT[i]==TRUE)  printf("%u ", i);
//         else                     printf("-%u ", i);
//     }
//     printf("\n");
//     return 0;
// }
// static int unsatisfiable(void){
//     printf("UNSAT\n");
//     return 0;
// }

// /****** set / unset ******/
// static int set_literal(int L, enum decision_type t){
//     int v=my_abs(L);
//     if(ASSIGNMENT[v]!=UNASSIGNED){
//         // conflict if contradictory
//         int assigned=(ASSIGNMENT[v]==TRUE)?1:0;
//         int want=(L>0)?1:0;
//         if(assigned!=want){
// #if DEBUG_CONFLICT
//             fprintf(stderr,"CONFLICT: var %u was %s, tried set %s\n",
//                     v,(assigned?"TRUE":"FALSE"),(want?"TRUE":"FALSE"));
// #endif
//             return 0; 
//         }
//         return 1; 
//     }
// #if DEBUG_PRINT
//     fprintf(stderr,"[set_literal] var=%u => %s, type=%d, stack=%u\n",
//             v,(L>0?"TRUE":"FALSE"),t,N_DECISION_STACK);
// #endif
//     ASSIGNMENT[v]=(L>0)?TRUE:FALSE;
//     DECISION_STACK[N_DECISION_STACK].var=v;
//     DECISION_STACK[N_DECISION_STACK].type=t;
//     N_DECISION_STACK++;

//     int opp=-L;
//     struct clause_list*cl=clauses_touching(opp);
//     for(int i=0;i<cl->n_clauses;i++){
//         struct clause*C=cl->clauses[i];
//         C->n_zeros++;
//         if(C->n_zeros==C->n_lits){
// #if DEBUG_CONFLICT
//             fprintf(stderr,"CONFLICT: Clause fully false => ");
//             for(int j=0;j<C->n_lits;j++){
//                 fprintf(stderr,"%d ", C->lits[j]);
//             }
//             fprintf(stderr,"\n");
// #endif
//             return 0; 
//         }
//     }
//     return 1;
// }

// static void unset_latest_assignment(void){
//     N_DECISION_STACK--;
//     unsigned v=DECISION_STACK[N_DECISION_STACK].var;
//     enum decision_type t=DECISION_STACK[N_DECISION_STACK].type;
//     int L=(ASSIGNMENT[v]==TRUE)?(int)v:-(int)v;
// #if DEBUG_PRINT
//     fprintf(stderr,"[unset_latest_assignment] var=%u was=%s type=%d => stack->%u\n",
//             v,(ASSIGNMENT[v]==TRUE?"TRUE":"FALSE"),t,N_DECISION_STACK);
// #endif
//     ASSIGNMENT[v]=UNASSIGNED;
//     int opp=-L;
//     struct clause_list*cw=clauses_touching(opp);
//     for(int i=0;i<cw->n_clauses;i++){
//         cw->clauses[i]->n_zeros--;
//     }
// }

// /****** Append clause ******/
// static void append_clause(int*arr,int n){
//     if(TOTAL_CLAUSES==MAX_CLAUSES){
//         MAX_CLAUSES=(MAX_CLAUSES<8)?8:(MAX_CLAUSES*2);
//         CLAUSES=realloc(CLAUSES,MAX_CLAUSES*sizeof(struct clause));
//     }
//     struct clause*C=&CLAUSES[TOTAL_CLAUSES++];
//     C->lits=NULL;
//     C->n_lits=0;
//     C->n_zeros=0;
//     for(int i=0;i<n;i++){
//         append_field((*C),lits)=arr[i];
//     }
//     for(int i=0;i<n;i++){
//         struct clause_list*cl=clauses_touching(arr[i]);
//         append_field((*cl),clauses)=C;
//     }
//     // init n_zeros
//     int z=0;
//     for(int i=0;i<n;i++){
//         int L=arr[i];
//         int v=my_abs(L);
//         if(ASSIGNMENT[v]!=UNASSIGNED){
//             int assignedFalse=((L>0 && ASSIGNMENT[v]==FALSE)||
//                                (L<0 && ASSIGNMENT[v]==TRUE));
//             if(assignedFalse) z++;
//         }
//     }
//     C->n_lits=n;
//     C->n_zeros=z;
// }

// /****** BCP ******/
// static int bcp(void){
//     int changed=1;
//     while(changed){
//         changed=0;
//         for(unsigned i=0;i<TOTAL_CLAUSES;i++){
//             struct clause*C=&CLAUSES[i];
//             if(C->n_zeros+1==C->n_lits){
//                 // check if satisfied
//                 int sat=0;
//                 for(int j=0;j<C->n_lits;j++){
//                     if(is_lit_true(C->lits[j])){sat=1;break;}
//                 }
//                 if(sat) continue;
//                 // force leftover => true
//                 for(int j=0;j<C->n_lits;j++){
//                     int L=C->lits[j];
//                     int v=my_abs(L);
//                     if(ASSIGNMENT[v]==UNASSIGNED){
// #if DEBUG_PRINT
//                         fprintf(stderr,"[bcp] forcing lit=%d => TRUE\n",L);
// #endif
//                         if(!set_literal(L,IMPLIED)){
//                             return 0;
//                         }
//                         changed=1;
//                         break;
//                     }
//                 }
//             }
//         }
//     }
//     return 1;
// }

// /****** record_conflict_clause ******/

// /*
//  We gather only the assignments from the *current* top-level block:
//    from the last TRIED_ONE_WAY index +1 to the end.
//  If we can't find a TRIED_ONE_WAY, but we still have a non-empty stack,
//    that means we popped the top decision but older decisions remain => gather the entire stack,
//    so we can do a deeper backjump.
//  If the entire stack is empty => conflict at level0 => truly unsat => skip learning.
// */
// static void record_conflict_clause(void){
//     // find last TRIED_ONE_WAY
//     int idx = N_DECISION_STACK-1;
//     while(idx>=0 && DECISION_STACK[idx].type!=TRIED_ONE_WAY){
//         idx--;
//     }
//     if(idx<0){
//         // no top-level found
//         if(N_DECISION_STACK==0){
//             // conflict at level0 => unsat => skip
// #if DEBUG_PRINT
//             fprintf(stderr,"[record_conflict_clause] conflict at level0 => unsat => skip\n");
// #endif
//             return;
//         }
//         // else gather entire stack => deeper backjump
// #if DEBUG_PRINT
//         fprintf(stderr,"[record_conflict_clause] no top-level => gather entire partial stack\n");
// #endif
//         int count = N_DECISION_STACK;
//         int *arr=malloc(count*sizeof(int));
//         int w=0;
//         for(int s=0; s<(int)N_DECISION_STACK; s++){
//             unsigned v=DECISION_STACK[s].var;
//             int sign=(ASSIGNMENT[v]==TRUE)?-1:1; 
//             arr[w++]= sign*(int)v;
//         }
//         append_clause(arr, w);
//         free(arr);
//         return;
//     }
//     // otherwise, gather from idx+1..end
//     int firstImplied = idx+1;
//     if(firstImplied>= (int)N_DECISION_STACK){
//         // no implied => single-literal flipping the top-level decision
//         unsigned v=DECISION_STACK[idx].var;
//         int sign=(ASSIGNMENT[v]==TRUE)?-1:1;
//         int arr[1]; arr[0]=sign*(int)v;
// #if DEBUG_PRINT
//         fprintf(stderr,"[record_conflict_clause] single-literal flipping var=%u\n", v);
// #endif
//         append_clause(arr,1);
//         return;
//     }
//     // gather top-level block
//     int count = N_DECISION_STACK - firstImplied;
//     int *arr=malloc(count*sizeof(int));
//     int w=0;
//     for(int s=firstImplied; s<(int)N_DECISION_STACK;s++){
//         unsigned v=DECISION_STACK[s].var;
//         int sign=(ASSIGNMENT[v]==TRUE)?-1:1;
//         arr[w++]= sign*(int)v;
//     }
// #if DEBUG_PRINT
//     fprintf(stderr,"[record_conflict_clause] top-level => (");
//     for(int i=0;i<w;i++){
//         fprintf(stderr,"%d ", arr[i]);
//     }
//     fprintf(stderr,")\n");
// #endif
//     append_clause(arr,w);
//     free(arr);
// }

// /****** Resolve conflict ******/
// static int resolveConflict(void){
//     // record new clause
//     record_conflict_clause();

//     while(1){
//         // pop implied
//         while(N_DECISION_STACK>0 &&
//               DECISION_STACK[N_DECISION_STACK-1].type!=TRIED_ONE_WAY){
//             unset_latest_assignment();
//         }
//         if(!N_DECISION_STACK){
//             // conflict at level0 => unsat
//             return 0;
//         }
//         // flip
//         unsigned v=DECISION_STACK[N_DECISION_STACK-1].var;
//         unset_latest_assignment();

//         // if it was false => now true, etc.
//         int newVal = (ASSIGNMENT[v]==TRUE)?0:1; 
//         // but we unassigned it => the old assignment is what it was
//         // we do 'flip'
//         int L = (newVal)? (int)v : -(int)v;
//         if(!set_literal(L, TRIED_BOTH_WAYS)){
//             // immediate conflict => loop again
//             continue;
//         }
//         // do BCP
//         while(!bcp()){
//             record_conflict_clause();
//             goto conflict_again;
//         }
//         return 1;
//     conflict_again:;
//     }
// }

// /****** Decide: pick random unassigned => false ******/
// static int decide(void){
//     // gather unassigned
//     int *u=malloc(sizeof(int)*(N_VARS-1));
//     int nu=0;
//     for(unsigned i=1;i<N_VARS;i++){
//         if(ASSIGNMENT[i]==UNASSIGNED){
//             u[nu++]=i;
//         }
//     }
//     if(!nu){
//         free(u);
//         return 0; // done
//     }
//     int pick = rand()%nu;
//     int var= u[pick];
//     free(u);
// #if DEBUG_PRINT
//     fprintf(stderr,"[decide] var=%u => FALSE\n", var);
// #endif
//     if(!set_literal(-var, TRIED_ONE_WAY)){
//         return 0; 
//     }
//     return 1;
// }

// /****** Check all clauses ******/
// static int checkAllClauses(void){
//     for(unsigned i=0;i<TOTAL_CLAUSES;i++){
//         struct clause*C=&CLAUSES[i];
//         int sat=0;
//         for(int j=0;j<C->n_lits;j++){
//             if(is_lit_true(C->lits[j])){sat=1;break;}
//         }
//         if(!sat) return 0; 
//     }
//     return 1;
// }

// /****** MAIN ******/
// int main(int argc,char**argv){
//     LOG_XCHECK=(argc>1);
//     srand(time(NULL));

//     // skip c lines
//     for(char c;(c=getc(stdin))=='c';){
//         while(getc(stdin)!='\n');
//     }
//     assert(scanf(" cnf %u %u\n",&N_VARS,&N_CLAUSES)==2);
//     N_VARS++;
//     MAX_CLAUSES=N_CLAUSES*2;
//     CLAUSES=calloc(MAX_CLAUSES,sizeof(struct clause));
//     TOTAL_CLAUSES=N_CLAUSES;

//     ASSIGNMENT=malloc(N_VARS*sizeof(*ASSIGNMENT));
//     for(unsigned i=0;i<N_VARS;i++){
//         ASSIGNMENT[i]=UNASSIGNED;
//     }
//     DECISION_STACK=calloc(N_VARS,sizeof(struct decision));
//     LIT_TO_CLAUSES=calloc(2*N_VARS,sizeof(struct clause_list));

//     // read original
//     for(unsigned i=0;i<N_CLAUSES;i++){
//         while(1){
//             int L;
//             assert(scanf("%d",&L)==1);
//             if(!L) break;
//             append_field(CLAUSES[i],lits)=L;
//         }
//         // watchers
//         for(int j=0;j<CLAUSES[i].n_lits;j++){
//             struct clause_list*cl=clauses_touching(CLAUSES[i].lits[j]);
//             append_field((*cl),clauses)=&CLAUSES[i];
//         }
//         // n_zeros
//         int z=0;
//         for(int j=0;j<CLAUSES[i].n_lits;j++){
//             int LL=CLAUSES[i].lits[j];
//             int vv=my_abs(LL);
//             if(ASSIGNMENT[vv]!=UNASSIGNED){
//                 int assignedFalse=((LL>0 && ASSIGNMENT[vv]==FALSE)
//                                    ||(LL<0 && ASSIGNMENT[vv]==TRUE));
//                 if(assignedFalse) z++;
//             }
//         }
//         CLAUSES[i].n_zeros=z;
//     }

//     // initial BCP
//     if(!bcp()){
//         record_conflict_clause();
//         if(!resolveConflict()){
//             return unsatisfiable();
//         }
//     }

//     // main loop
//     while(1){
//         if(!decide()){
//             if(checkAllClauses()){
//                 return satisfiable();
//             } else {
//                 record_conflict_clause();
//                 if(!resolveConflict()){
//                     return unsatisfiable();
//                 }
//             }
//         }
//         while(!bcp()){
//             record_conflict_clause();
//             if(!resolveConflict()){
//                 return unsatisfiable();
//             }
//         }
//     }
//     return 0;
// }





// THIS WOKRS!!!!

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <assert.h>

// /******************************************************************************
//  * A Minimal DPLL “Part 1” SAT Solver
//  * 
//  * Features:
//  *  - Reads DIMACS from stdin
//  *  - Variables are 1..N
//  *  - Decides in ascending order, always sets variable to false first
//  *  - Does naive unit propagation (BCP) with an n_zeros counter
//  *  - Backtracks on conflict: flips the most recent TRIED_ONE_WAY variable
//  *  - Prints "Decide: -X" lines for cross-check
//  *  - No clause learning; no advanced heuristics
//  ******************************************************************************/

// /*** Global Data ***/
// static unsigned N_VARS = 0, N_CLAUSES = 0;

// enum assignment {
//     UNASSIGNED = -1,
//     FALSE      = 0,
//     TRUE       = 1
// };
// static enum assignment *ASSIGNMENT = NULL;

// struct clause {
//     int  *lits;       // literals array
//     int   n_lits; 
//     int   n_zeros;
// };

// static struct clause *CLAUSES = NULL;

// // For backtracking, record  var assignment in stack
// enum decision_type {
//     IMPLIED       = 0,
//     TRIED_ONE_WAY = 1,
//     TRIED_BOTH_WAYS = 2
// };
// struct decision {
//     unsigned var;
//     enum decision_type type;
// };
// static struct decision *DECISION_STACK = NULL;
// static unsigned N_DECISION_STACK = 0;

// struct clause_list {
//     struct clause **arr;
//     int size;
// };

// static struct clause_list *LIT_TO_CLAUSES = NULL;

// static inline int my_abs(int x) { return (x < 0) ? -x : x; }

// // Convert literal L to  index for LIT_TO_CLAUSES
// static inline int lit2idx(int L) {
//     return 2 * my_abs(L) + (L > 0);
// }

// static inline struct clause_list *clauses_touching(int L) {
//     return &LIT_TO_CLAUSES[lit2idx(L)];
// }

// // Check if L is SAT under current assignment
// static inline int is_lit_true(int L) {
//     int v = my_abs(L);
//     if (ASSIGNMENT[v] == UNASSIGNED) return 0;
//     return (ASSIGNMENT[v] == (L > 0));
// }

// // Print assignment if SAT
// static int satisfiable() {
//     printf("SAT\n");
//     for (unsigned i = 1; i < N_VARS; i++) {
//         if (ASSIGNMENT[i] == TRUE)  printf("%u ", i);
//         else                        printf("-%u ", i);
//     }
//     printf("\n");
//     return 0;
// }

// // If UNSAT
// static int unsatisfiable() {
//     printf("UNSAT\n");
//     return 0;
// }

// /******************************************************************************
//  * set_literal / unset_latest_assignment
//  *
//  * If set_literal triggers a conflict, return 0. Otherwise, 1.
//  ******************************************************************************/
// static int set_literal(int L, enum decision_type t) {
//     int v = my_abs(L);
//     // If var already assigned, check if conflict
//     if (ASSIGNMENT[v] != UNASSIGNED) {
//         int assignedVal = (ASSIGNMENT[v] == TRUE) ? 1 : 0;
//         int wantVal = (L > 0) ? 1 : 0;
//         if (assignedVal != wantVal) {
//             return 0; // conflict
//         }
//         return 1; // no conflict
//     }

//     // Otherwise, assign
//     ASSIGNMENT[v] = (L > 0) ? TRUE : FALSE;

//     if (t == TRIED_ONE_WAY || t == TRIED_BOTH_WAYS) {
//         if (t == TRIED_ONE_WAY) {
//             // fprintf(stderr, "Decide: %d\n", L);
//         } else {
//             // fprintf(stderr, "Decide: %d\n", L);
//         }
//     }

//     // Push on the decision stack
//     DECISION_STACK[N_DECISION_STACK].var = v;
//     DECISION_STACK[N_DECISION_STACK].type = t;
//     N_DECISION_STACK++;

//     // Increment n_zeros for clauses containing opposite literal
//     int opp = -L;
//     struct clause_list *cl = clauses_touching(opp);
//     for (int i = 0; i < cl->size; i++) {
//         struct clause *C = cl->arr[i];
//         C->n_zeros++;
//         if (C->n_zeros == C->n_lits) {
//             // conflict: clause is false
//             return 0;
//         }
//     }
//     return 1;
// }

// static void unset_latest_assignment(void) {
//     // Pop from decision stack
//     N_DECISION_STACK--;
//     unsigned v = DECISION_STACK[N_DECISION_STACK].var;

//     int L = (ASSIGNMENT[v] == TRUE) ? (int)v : -(int)v;
//     ASSIGNMENT[v] = UNASSIGNED;

//     // Decrement n_zeros for all clauses containing -L
//     int opp = -L;
//     struct clause_list *cw = clauses_touching(opp);
//     for (int i = 0; i < cw->size; i++) {
//         cw->arr[i]->n_zeros--;
//     }
// }

// /******************************************************************************
//  * bcp(): naive unit propagation
//  * Return 1 if no conflict, else 0
//  ******************************************************************************/
// static int bcp(void) {
//     int changed = 1;
//     while (changed) {
//         changed = 0;
//         for (unsigned i = 0; i < N_CLAUSES; i++) {
//             struct clause *C = &CLAUSES[i];

//             if ((C->n_zeros + 1) != C->n_lits) {
//                 continue;
//             }
//             // Check if SAT
//             int satisfied = 0;
//             for (int j = 0; j < C->n_lits; j++) {
//                 if (is_lit_true(C->lits[j])) {
//                     satisfied = 1;
//                     break;
//                 }
//             }
//             if (satisfied) continue;

//             // If unsat, find the unassigned lit
//             int forced_lit = 0;
//             for (int j = 0; j < C->n_lits; j++) {
//                 int L = C->lits[j];
//                 if (ASSIGNMENT[my_abs(L)] == UNASSIGNED) {
//                     forced_lit = L;
//                     break;
//                 }
//             }
//             if (!forced_lit) {
//                 // all assigned false
//                 return 0;
//             }
//             // else, set forced_lit
//             if (!set_literal(forced_lit, IMPLIED)) {
//                 return 0;
//             }
//             changed = 1;
//         }
//     }
//     return 1;
// }

// /******************************************************************************
//  * resolveConflict():
//  *  - Unwind the stack until we find a TRIED_ONE_WAY var => flip it
//  *  - If none found => unsatisfiable
//  ******************************************************************************/
// static int resolveConflict(void) {
//     while (1) {
//         while (N_DECISION_STACK > 0 &&
//                (DECISION_STACK[N_DECISION_STACK - 1].type != TRIED_ONE_WAY)) {
//             unset_latest_assignment();
//         }
//         if (N_DECISION_STACK == 0) {
//             // no var to flip --> unsat
//             return 0;
//         }

//         // flip variable
//         unsigned v = DECISION_STACK[N_DECISION_STACK - 1].var;
//         unset_latest_assignment(); // unassign it

//         // flipping false->true or true->false
//         int L = (int)v;  // set literal positive
//         if (!set_literal(L, TRIED_BOTH_WAYS)) {
//             //  conflict -->  continue to unwind
//             continue;
//         }

//         // BCP again
//         while (!bcp()) {
//             if (!resolveConflict()) {
//                 return 0;
//             }
//         }
//         return 1; // success
//     }
// }

// /******************************************************************************
//  * decide(): pick  next unassigned variable in ascending order, set to false
//  ******************************************************************************/
// static int decide(void) {
//     for (unsigned i = 1; i < N_VARS; i++) {
//         if (ASSIGNMENT[i] == UNASSIGNED) {
//             int L = -(int)i;
//             if (!set_literal(L, TRIED_ONE_WAY)) {
//                 return 0; //conflict --> return
//             }
//             return 1; // decided
//         }
//     }
//     // no unassigned left
//     return 0;
// }

// /******************************************************************************
//  * final check
//  ******************************************************************************/
// static int checkAllClauses(void) {
//     for (unsigned i = 0; i < N_CLAUSES; i++) {
//         struct clause *C = &CLAUSES[i];
//         int sat = 0;
//         for (int j = 0; j < C->n_lits && !sat; j++) {
//             if (is_lit_true(C->lits[j])) {
//                 sat = 1;
//             }
//         }
//         if (!sat) return 0; // unsat clause -->  error
//     }
//     return 1;
// }

// /******************************************************************************
//  * main
//  ******************************************************************************/
// int main(int argc, char **argv) {
//     // int doDebug = (argc > 1);

//     for (char c; (c = getc(stdin)) == 'c'; ) {
//         while (getc(stdin) != '\n');
//     }

//     // read p cnf
//     assert(scanf(" cnf %u %u\n", &N_VARS, &N_CLAUSES) == 2);
//     N_VARS++;

//     // allocate
//     ASSIGNMENT = malloc(N_VARS * sizeof(*ASSIGNMENT));
//     for (unsigned i = 0; i < N_VARS; i++) {
//         ASSIGNMENT[i] = UNASSIGNED;
//     }
//     CLAUSES = calloc(N_CLAUSES, sizeof(struct clause));
//     DECISION_STACK = calloc(N_VARS, sizeof(struct decision));

//     LIT_TO_CLAUSES = calloc(2*N_VARS, sizeof(struct clause_list));

//     // read the clauses
//     for (unsigned i = 0; i < N_CLAUSES; i++) {
//         // store in CLAUSES[i]
//         // parse utnil '0'
//         int L;
//         int arrayCap = 8;
//         int arrayLen = 0;
//         int *temp = malloc(arrayCap * sizeof(int));

//         while (1) {
//             assert(scanf("%d", &L) == 1);
//             if (!L) break;

//             // skip duplicates
//             int repeat = 0;
//             for (int x = 0; x < arrayLen && !repeat; x++) {
//                 if (temp[x] == L) {
//                     repeat = 1;
//                 }
//             }
//             if (repeat) continue;

//             if (arrayLen == arrayCap) {
//                 arrayCap *= 2;
//                 temp = realloc(temp, arrayCap * sizeof(int));
//             }
//             temp[arrayLen++] = L;
//         }

//         // store in CLAUSES[i]
//         CLAUSES[i].lits = malloc(arrayLen * sizeof(int));
//         CLAUSES[i].n_lits = arrayLen;
//         CLAUSES[i].n_zeros = 0;
//         for (int j = 0; j < arrayLen; j++) {
//             CLAUSES[i].lits[j] = temp[j];
//         }
//         free(temp);

//         // LIT_TO_CLAUSES
//         for (int j = 0; j < arrayLen; j++) {
//             int idx = lit2idx(CLAUSES[i].lits[j]);
//             struct clause_list *cl = &LIT_TO_CLAUSES[idx];

//             cl->arr = realloc(cl->arr, (cl->size + 1)*sizeof(struct clause*));
//             cl->arr[cl->size] = &CLAUSES[i];
//             cl->size++;
//         }

//         // compute initial n_zeros
//         for (int j = 0; j < arrayLen; j++) {
//             int v = my_abs(CLAUSES[i].lits[j]);
//             int signClause = (CLAUSES[i].lits[j] > 0) ? 1 : 0;
//             if (ASSIGNMENT[v] != UNASSIGNED) {
//                 int signAssigned = (ASSIGNMENT[v] == TRUE) ? 1 : 0;
//                 if (signClause != signAssigned) {
//                     CLAUSES[i].n_zeros++;
//                 }
//             }
//         }
//     }

//     // initial BCP
//     if (!bcp()) {
//         if (!resolveConflict()) {
//             return unsatisfiable();
//         }
//     }

//     // main loop
//     while (1) {
//         if (!decide()) {
//             if (checkAllClauses()) {
//                 return satisfiable();
//             } else {
//                 if (!resolveConflict()) {
//                     return unsatisfiable();
//                 }
//             }
//         }
//         while (!bcp()) {
//             if (!resolveConflict()) {
//                 return unsatisfiable();
//             }
//         }
//     }

//     return 0;
// }



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/******************************************************************************
 * DPLL SAT Solver for Part 1
 * Read DIMACS, variables from 1..N, decide in ascending order (set var to false first), 
 * naive unit propogation (BCP) with n_zeros counter, backtrack on conflict (flips TRIED_ONE_WAY),
 * print "Decide: -X" lines, no clause learning yet
 ******************************************************************************/

/*** Global Data ***/
static unsigned N_VARS = 0, N_CLAUSES = 0;

enum assignment {
    UNASSIGNED = -1,
    FALSE      = 0,
    TRUE       = 1
};
static enum assignment *ASSIGNMENT = NULL;

struct clause {
    int  *literals;  
    int   n_literals; 
    int   n_zeros;
};

static struct clause *CLAUSES = NULL;

// For backtracking, record var assignments in a stack
enum decision_type {
    IMPLIED       = 0,
    TRIED_ONE_WAY = 1,
    TRIED_BOTH_WAYS = 2
};
struct decision {
    unsigned var;
    enum decision_type type;
};
static struct decision *DECISION_STACK = NULL;
static unsigned N_DECISION_STACK = 0;

struct clause_list {
    struct clause **clauses; 
    int n_clauses;           
};

static struct clause_list *LIT_TO_CLAUSES = NULL;

/******************************************************************************
 * Helper Macros / Functions
 ******************************************************************************/

// Convert literal to index for LIT_TO_CLAUSES: -1->1, +1->2, -2->3, etc.
static inline int literal_to_id(int literal) {
    // We'll keep your "my_abs" logic inlined:
    int a = (literal < 0) ? -literal : literal;
    return 2 * a + (literal > 0);
}

// Check if L is SAT
static inline int is_lit_true(int L) {
    int v = (L < 0) ? -L : L; // abs(L)
    if (ASSIGNMENT[v] == UNASSIGNED) return 0;
    return (ASSIGNMENT[v] == (L > 0));
}

// Print assignment if SAT
static int satisfiable() {
    printf("SAT\n");
    for (unsigned i = 1; i < N_VARS; i++) {
        if (ASSIGNMENT[i] == TRUE)  printf("%u ", i);
        else                        printf("-%u ", i);
    }
    printf("\n");
    return 0;
}

// If UNSAT
static int unsatisfiable() {
    printf("UNSAT\n");
    return 0;
}

/******************************************************************************
 * set_literal / unset_latest_assignment
 * If set_literal triggers conflict, return 0; else, 1
 ******************************************************************************/

// Attempt to assign literal L. If we see a conflict, return 0.
static int set_literal(int L, enum decision_type t) {
    // If var already assigned, check if conflict
    int v = (L < 0) ? -L : L; 
    if (ASSIGNMENT[v] != UNASSIGNED) {
        int assignedVal = (ASSIGNMENT[v] == TRUE) ? 1 : 0;
        int wantVal = (L > 0) ? 1 : 0;
        if (assignedVal != wantVal) {
            return 0; // conflict
        }
        return 1; // no conflict
    }

    // Otherwise, assign
    ASSIGNMENT[v] = (L > 0) ? TRUE : FALSE;

    // If you want to print "Decide: L" for cross-check, you could do it here:
    // if (t == TRIED_ONE_WAY) {
    //     fprintf(stderr, "Decide: %d\n", L);
    // } else if (t == TRIED_BOTH_WAYS) {
    //     fprintf(stderr, "Decide: %d\n", L);
    // }

    // Push on the decision stack
    DECISION_STACK[N_DECISION_STACK].var = v;
    DECISION_STACK[N_DECISION_STACK].type = t;
    N_DECISION_STACK++;

    // Increment n_zeros for clauses containing opposite literal
    int opp = -L;
    struct clause_list *cl = &LIT_TO_CLAUSES[literal_to_id(opp)];
    for (int i = 0; i < cl->n_clauses; i++) {
        struct clause *C = cl->clauses[i];
        C->n_zeros++;
        if (C->n_zeros == C->n_literals) {
            // conflict: entire clause is false
            return 0;
        }
    }
    return 1;
}

// Undo the latest assignment
static void unset_latest_assignment(void) {
    // Pop from decision stack
    N_DECISION_STACK--;
    unsigned v = DECISION_STACK[N_DECISION_STACK].var;

    // figure out the literal we set
    int L = (ASSIGNMENT[v] == TRUE) ? (int)v : -(int)v;
    ASSIGNMENT[v] = UNASSIGNED;

    // Decrement n_zeros for all clauses containing -L
    int opp = -L;
    struct clause_list *cw = &LIT_TO_CLAUSES[literal_to_id(opp)];
    for (int i = 0; i < cw->n_clauses; i++) {
        cw->clauses[i]->n_zeros--;
    }
}

/******************************************************************************
 * bcp(): naive unit propagation
 * Return 1 if no conflict, else 0
 ******************************************************************************/
static int bcp(void) {
    int changed = 1;
    while (changed) {
        changed = 0;
        for (unsigned i = 0; i < N_CLAUSES; i++) {
            struct clause *C = &CLAUSES[i];

            if ((C->n_zeros + 1) != C->n_literals) {
                continue;
            }
            // Check if satisfied
            int satisfied = 0;
            for (int j = 0; j < C->n_literals; j++) {
                if (is_lit_true(C->literals[j])) {
                    satisfied = 1;
                    break;
                }
            }
            if (satisfied) continue;

            // If UNSAT, find unassigned literal
            int forced_lit = 0;
            for (int j = 0; j < C->n_literals; j++) {
                int L = C->literals[j];
                int absL = (L < 0) ? -L : L;
                if (ASSIGNMENT[absL] == UNASSIGNED) {
                    forced_lit = L;
                    break;
                }
            }
            if (!forced_lit) {
                // all assigned false
                return 0;
            }
            // else, set forced_lit
            if (!set_literal(forced_lit, IMPLIED)) {
                return 0;
            }
            changed = 1;
        }
    }
    return 1;
}

/******************************************************************************
 * resolveConflict(): unwind stack until we find TRIED_ONE_WAY var --> flip; if none found, UNSAT
 ******************************************************************************/
static int resolveConflict(void) {
    while (1) {
        while (N_DECISION_STACK > 0 &&
               (DECISION_STACK[N_DECISION_STACK - 1].type != TRIED_ONE_WAY)) {
            unset_latest_assignment();
        }
        if (N_DECISION_STACK == 0) {
            // no var to flip => unsat
            return 0;
        }

        // flip variable
        unsigned v = DECISION_STACK[N_DECISION_STACK - 1].var;
        unset_latest_assignment(); 

        // flipping false->true or true->false; set var positive
        int L = (int)v;  
        if (!set_literal(L, TRIED_BOTH_WAYS)) {
            // conflict => keep unwinding
            continue;
        }

        // do BCP again
        while (!bcp()) {
            if (!resolveConflict()) {
                return 0;
            }
        }
        return 1;
    }
}

/******************************************************************************
 * decide(): pick next unassigned variable in ascending order, set to false
 ******************************************************************************/
static int decide(void) {
    for (unsigned i = 1; i < N_VARS; i++) {
        if (ASSIGNMENT[i] == UNASSIGNED) {
            int L = -(int)i;
            if (!set_literal(L, TRIED_ONE_WAY)) {
                return 0;
            }
            return 1;
        }
    }
    return 0; 
}

/******************************************************************************
 * final check
 ******************************************************************************/
static int checkAllClauses(void) {
    for (unsigned i = 0; i < N_CLAUSES; i++) {
        struct clause *C = &CLAUSES[i];
        int sat = 0;
        for (int j = 0; j < C->n_literals && !sat; j++) {
            if (is_lit_true(C->literals[j])) {
                sat = 1;
            }
        }
        if (!sat) return 0;
    }
    return 1;
}

/******************************************************************************
 * main
 ******************************************************************************/
int main(int argc, char **argv) {
    // Skip comment lines
    for (char c; (c = getc(stdin)) == 'c'; ) {
        while (getc(stdin) != '\n');
    }

    // read p cnf
    assert(scanf(" cnf %u %u\n", &N_VARS, &N_CLAUSES) == 2);
    N_VARS++;

    // allocate
    ASSIGNMENT     = malloc(N_VARS * sizeof(*ASSIGNMENT));
    CLAUSES        = calloc(N_CLAUSES, sizeof(struct clause));
    DECISION_STACK = calloc(N_VARS, sizeof(struct decision));
    LIT_TO_CLAUSES = calloc(2*N_VARS, sizeof(struct clause_list));

    for (unsigned i = 0; i < N_VARS; i++) {
        ASSIGNMENT[i] = UNASSIGNED;
    }

    // read clauses
    for (unsigned i = 0; i < N_CLAUSES; i++) {
        // parse until '0'
        int L;
        int arrayCap = 8;
        int arrayLen = 0;
        int *temp = malloc(arrayCap * sizeof(int));

        while (1) {
            assert(scanf("%d", &L) == 1);
            if (!L) break;

            // skip duplicates
            int repeat = 0;
            for (int x = 0; x < arrayLen && !repeat; x++) {
                if (temp[x] == L) {
                    repeat = 1;
                }
            }
            if (repeat) continue;

            if (arrayLen == arrayCap) {
                arrayCap *= 2;
                temp = realloc(temp, arrayCap * sizeof(int));
            }
            temp[arrayLen++] = L;
        }

        // store in CLAUSES[i]
        CLAUSES[i].literals   = malloc(arrayLen * sizeof(int));
        CLAUSES[i].n_literals = arrayLen;
        CLAUSES[i].n_zeros    = 0;

        for (int j = 0; j < arrayLen; j++) {
            CLAUSES[i].literals[j] = temp[j];
        }
        free(temp);

        // populate LIT_TO_CLAUSES
        for (int j = 0; j < arrayLen; j++) {
            int idx = literal_to_id(CLAUSES[i].literals[j]);
            struct clause_list *cl = &LIT_TO_CLAUSES[idx];
            cl->clauses = realloc(cl->clauses, (cl->n_clauses+1)*sizeof(struct clause*));
            cl->clauses[cl->n_clauses] = &CLAUSES[i];
            cl->n_clauses++;
        }

        // compute initial n_zeros
        for (int j = 0; j < arrayLen; j++) {
            int var = (CLAUSES[i].literals[j] < 0) ? -CLAUSES[i].literals[j] : CLAUSES[i].literals[j];
            if (ASSIGNMENT[var] != UNASSIGNED) {
                // if sign mismatch => increment n_zeros
                int assignedVal = (ASSIGNMENT[var] == TRUE) ? 1 : 0;
                int wantVal     = (CLAUSES[i].literals[j] > 0) ? 1 : 0;
                if (assignedVal != wantVal) {
                    CLAUSES[i].n_zeros++;
                }
            }
        }
    }

    // initial BCP
    if (!bcp()) {
        if (!resolveConflict()) {
            return unsatisfiable();
        }
    }

    // main loop
    while (1) {
        if (!decide()) {
            if (checkAllClauses()) {
                return satisfiable();
            } else {
                if (!resolveConflict()) {
                    return unsatisfiable();
                }
            }
        }
        while (!bcp()) {
            if (!resolveConflict()) {
                return unsatisfiable();
            }
        }
    }
    return 0;
}
