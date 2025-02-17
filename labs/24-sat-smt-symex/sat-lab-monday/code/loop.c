#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define append_field(OBJ, FIELD) (*({ \
    (OBJ).FIELD = realloc((OBJ).FIELD, (++((OBJ).n_##FIELD)) * sizeof((OBJ).FIELD[0])); \
    (OBJ).FIELD + ((OBJ).n_##FIELD - 1); \
}))

/****** GLOBAL DATA STRUCTURES ******/

int LOG_XCHECK = 0;
#define xprintf(...) { if (LOG_XCHECK) { fprintf(stderr, __VA_ARGS__); } }

unsigned N_VARS = 0, N_CLAUSES = 0;

enum assignment {
    UNASSIGNED  = -1,
    FALSE       = 0,
    TRUE        = 1,
};

struct clause {
    int *literals;
    int  n_literals;
    int  n_zeros;
};
struct clause *CLAUSES = NULL;

enum assignment *ASSIGNMENT = NULL;

enum decision_type {
    IMPLIED         = 0,
    TRIED_ONE_WAY   = 1,
    TRIED_BOTH_WAYS = 2,
};
struct decision {
    unsigned var;
    enum decision_type type;
};
struct decision *DECISION_STACK = NULL;
unsigned N_DECISION_STACK = 0;

struct clause_list {
    struct clause **clauses;
    int n_clauses;
};
struct clause_list *LIT_TO_CLAUSES = NULL;

/****** HELPER METHODS ******/

int abs(int x) { return (x < 0) ? -x : x; }

int literal_to_id(int literal) {
    // negative lit => even index; positive lit => odd index
    return 2 * abs(literal) + (literal > 0);
}
struct clause_list *clauses_touching(int literal) {
    return &LIT_TO_CLAUSES[literal_to_id(literal)];
}

int is_literal_true(int lit) {
    int var = abs(lit);
    return (ASSIGNMENT[var] != UNASSIGNED) && (ASSIGNMENT[var] == (lit > 0));
}

int satisfiable() {
    printf("SAT\n");
    for (int i = 1; i < (int)N_VARS; i++) {
        printf("%d ", (ASSIGNMENT[i] == TRUE) ? i : -i);
    }
    printf("\n");
    return 0;
}

int unsatisfiable() {
    printf("UNSAT\n");
    return 0;
}

/****** KEY OPERATIONS ******/

int set_literal(int literal, enum decision_type type) {
    int var = abs(literal);
    assert(ASSIGNMENT[var] == UNASSIGNED);

    // set assignment
    ASSIGNMENT[var] = (literal > 0) ? TRUE : FALSE;
    // push onto decision stack
    DECISION_STACK[N_DECISION_STACK].var = var;
    DECISION_STACK[N_DECISION_STACK].type = type;
    N_DECISION_STACK++;

    // increment n_zeros for clauses that contain the opposite literal
    int opp = -literal;
    struct clause_list *lst = clauses_touching(opp);
    for (int i = 0; i < lst->n_clauses; i++) {
        struct clause *c = lst->clauses[i];
        c->n_zeros++;
        if (c->n_zeros == c->n_literals) {
            // conflict
            return 0;
        }
    }
    return 1;
}

void unset_latest_assignment() {
    // pop from stack
    N_DECISION_STACK--;
    unsigned var = DECISION_STACK[N_DECISION_STACK].var;

    int literal = (ASSIGNMENT[var] == TRUE) ? var : -var;
    ASSIGNMENT[var] = UNASSIGNED;

    // decrement n_zeros for clauses containing -literal
    int opp = -literal;
    struct clause_list *lst = clauses_touching(opp);
    for (int i = 0; i < lst->n_clauses; i++) {
        lst->clauses[i]->n_zeros--;
    }
}

/****** DP METHODS ******/

int decide() {
    int v = 0;
    for (int i = 1; i < (int)N_VARS; i++) {
        if (ASSIGNMENT[i] == UNASSIGNED) {
            v = i;
            break;
        }
    }
    if (v == 0) {
        // all assigned => solved
        return 0;
    }
    // set it to FALSE first
    int ok = set_literal(-v, TRIED_ONE_WAY);
    assert(ok && "Conflict during decide() should not happen if BCP was complete!");
    xprintf("Decide: %d\n", -v);
    return 1;
}

int bcp() {
    int any_change = 0;
rescan:
    for (int i = 0; i < (int)N_CLAUSES; i++) {
        struct clause *cl = &CLAUSES[i];
        // If exactly one literal not assigned 0 => forced
        if ((cl->n_zeros + 1) == cl->n_literals) {
            // check if it's already satisfied
            int foundTrue = 0;
            for (int j = 0; j < cl->n_literals; j++) {
                if (is_literal_true(cl->literals[j])) {
                    foundTrue = 1;
                    break;
                }
            }
            if (foundTrue) continue;

            // otherwise, set the single unassigned literal to 1
            for (int j = 0; j < cl->n_literals; j++) {
                int lit = cl->literals[j];
                int var = abs(lit);
                if (ASSIGNMENT[var] == UNASSIGNED) {
                    if (!set_literal(lit, IMPLIED)) {
                        return 0;
                    }
                    any_change = 1;
                    goto rescan; // re-check from start
                }
            }
            // if we got here, it's conflict or satisfied
        }
    }
    return 1; // no immediate conflict
}

int resolveConflict() {
    // pop until we find TRIED_ONE_WAY to flip
    while (N_DECISION_STACK > 0) {
        if (DECISION_STACK[N_DECISION_STACK - 1].type == TRIED_ONE_WAY) {
            break;
        }
        unset_latest_assignment();
    }

    if (N_DECISION_STACK == 0) {
        return 0; // unsat
    }
    // flip top
    unsigned var = DECISION_STACK[N_DECISION_STACK - 1].var;
    DECISION_STACK[N_DECISION_STACK - 1].type = TRIED_BOTH_WAYS;
    unset_latest_assignment();

    int new_val = !ASSIGNMENT[var]; // if it was false -> true, etc.
    if (!set_literal(new_val ? var : -var, TRIED_BOTH_WAYS)) {
        return 0; // immediate conflict
    }
    return 1;
}

/*******************************************/
/* The main solver loop, extracted into a  */
/* "solve_once()" helper function.         */
/*******************************************/

int solve_once(void) {
    // initial BCP to handle any unit clauses
    if (!bcp()) {
        unsatisfiable();
        return 0; // unsat
    }

    while (1) {
        if (!decide()) {
            satisfiable();
            return 1; // sat
        }
        while (!bcp()) {
            if (!resolveConflict()) {
                unsatisfiable();
                return 0; // unsat
            }
        }
    }
}

/*******************************************/
/* Main: read the puzzle, solve it 100000x */
/*******************************************/

int main(int argc, char **argv) {
    LOG_XCHECK = (argc > 1);

    // skip lines that start with 'c'
    for (char c; (c = getc(stdin)) == 'c'; )
        while (getc(stdin) != '\n');

    // read "p cnf N_VARS N_CLAUSES"
    assert(scanf(" cnf %u %u\n", &N_VARS, &N_CLAUSES) == 2);
    N_VARS++;

    ASSIGNMENT = malloc(N_VARS * sizeof(*ASSIGNMENT));
    DECISION_STACK = calloc(N_VARS, sizeof(*DECISION_STACK));
    CLAUSES = calloc(N_CLAUSES, sizeof(*CLAUSES));
    LIT_TO_CLAUSES = calloc(2 * N_VARS, sizeof(*LIT_TO_CLAUSES));

    // read each clause
    for (unsigned i = 0; i < N_CLAUSES; i++) {
        int literal = 0;
        scanf("%d", &literal);
        while (literal != 0) {
            // deduplicate
            int repeat = 0;
            for (int j = 0; j < CLAUSES[i].n_literals && !repeat; j++) {
                if (CLAUSES[i].literals[j] == literal) {
                    repeat = 1;
                }
            }
            if (!repeat) {
                append_field(CLAUSES[i], literals) = literal;
                append_field(*clauses_touching(literal), clauses) = &CLAUSES[i];
            }
            scanf("%d", &literal);
        }
    }

    // We'll solve the same puzzle 100,000 times in one process
    for (int run = 0; run < 100000; run++) {
        // reset assignment
        memset(ASSIGNMENT, -1, N_VARS * sizeof(*ASSIGNMENT));
        // reset decision stack
        N_DECISION_STACK = 0;
        // reset each clause's n_zeros
        for (unsigned i = 0; i < N_CLAUSES; i++) {
            CLAUSES[i].n_zeros = 0;
        }

        solve_once();
        // if you want to see which iteration you're on:
        // fprintf(stderr, "Finished run %d\n", run);
    }

    return 0;
}