#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define append_field(OBJ, FIELD) (*({ \
    (OBJ).FIELD = realloc((OBJ).FIELD, (++((OBJ).n_##FIELD)) * sizeof((OBJ).FIELD[0])); \
    (OBJ).FIELD + ((OBJ).n_##FIELD - 1); \
}))

int LOG_XCHECK = 0;
#define xprintf(...) { if (LOG_XCHECK) { fprintf(stderr, __VA_ARGS__); } }

unsigned N_VARS = 0, N_CLAUSES = 0;

struct clause {
    int *literals, n_literals;
    int watching[2];
};
struct clause *CLAUSES = NULL;

enum assignment {
    UNASSIGNED  = -1,
    FALSE       = 0,
    TRUE        = 1,
};
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
    struct clause *clause;
    struct clause_list *next;
};
struct clause_list **LIT_TO_CLAUSES = NULL;

int *BCP_LIST = NULL;
unsigned N_BCP_LIST = 0;
char *IS_BCP_LISTED = NULL;

int literal_to_id(int literal) {
    return (2 * abs(literal)) + (literal > 0);
}

struct clause_list **clauses_watching(int literal) {
    return LIT_TO_CLAUSES + literal_to_id(literal);
}

int abs(int x) { return (x < 0) ? -x : x; }

int is_literal_true(int lit) {
    int var = abs(lit);
    return ASSIGNMENT[var] != UNASSIGNED && ASSIGNMENT[var] == (lit > 0);
}

int satisfiable() {
    printf("SAT\n");
    for (int i = 1; i < N_VARS; i++)
        printf("%d ", is_literal_true(i) ? i : -i);
    printf("\n");
    return 0;
}

int unsatisfiable() { printf("UNSAT\n"); return 0; }

void init_watcher(int clause_i, int which, int literal) {
    CLAUSES[clause_i].watching[which] = literal;
    struct clause_list *n = malloc(sizeof(*n));
    n->clause = &CLAUSES[clause_i];
    n->next = *clauses_watching(literal);
    *clauses_watching(literal) = n;
}

void queue_bcp(int literal) {
    int id = literal_to_id(literal);
    if (!IS_BCP_LISTED[id]) {
        BCP_LIST[N_BCP_LIST++] = literal;
        IS_BCP_LISTED[id] = 1;
    }
}

int dequeue_bcp() {
    int l = BCP_LIST[--N_BCP_LIST];
    IS_BCP_LISTED[literal_to_id(l)] = 0;
    return l;
}

int set_literal(int literal, enum decision_type type) {
    int var = abs(literal);
    ASSIGNMENT[var] = (literal > 0);
    DECISION_STACK[N_DECISION_STACK].var = var;
    DECISION_STACK[N_DECISION_STACK++].type = type;

    for (struct clause_list **w = clauses_watching(-literal); *w;) {
        struct clause *clause = (*w)->clause;
        int watch_id = (clause->watching[0] == -literal) ? 0 : 1;
        int other_watch = clause->watching[!watch_id];
        int new_watch_lit = 0;

        for (int i = 0; i < clause->n_literals; i++) {
            int c = clause->literals[i];
            if (c != other_watch && c != -literal && !is_literal_true(-c)) {
                new_watch_lit = c;
                break;
            }
        }

        if (new_watch_lit) {
            struct clause_list *cw = *w;
            *w = cw->next;
            cw->next = *clauses_watching(new_watch_lit);
            *clauses_watching(new_watch_lit) = cw;
            clause->watching[watch_id] = new_watch_lit;
            continue;
        } else {
            if (is_literal_true(-other_watch)) {
                return 0;
            } else if (is_literal_true(other_watch)) {
            } else {
                queue_bcp(other_watch);
            }
            w = &((*w)->next);
        }
    }
    return 1;
}

void unset_latest_assignment() {
    unsigned var = DECISION_STACK[--N_DECISION_STACK].var;
    ASSIGNMENT[var] = UNASSIGNED;
}

int decide() {
    int v;
    for (v = 1; v < N_VARS; v++) {
        if (ASSIGNMENT[v] == UNASSIGNED) {
            break;
        }
    }
    if (v == N_VARS)
        return 0;
    assert(set_literal(-v, TRIED_ONE_WAY));

    xprintf("Decide: %d\n", -v);
    return 1;
}

int bcp() {
    while (N_BCP_LIST) {
        if (!set_literal(dequeue_bcp(), IMPLIED))
            return 0;
    }
    return 1;
}

int resolveConflict() {
    while (N_BCP_LIST) {
        int l = BCP_LIST[--N_BCP_LIST];
        IS_BCP_LISTED[literal_to_id(l)] = 0;
    }
    while (N_DECISION_STACK) {
        if (DECISION_STACK[N_DECISION_STACK - 1].type == TRIED_ONE_WAY)
            break;
        unset_latest_assignment();
    }
    if (!N_DECISION_STACK)
        return 0;

    unsigned var = DECISION_STACK[N_DECISION_STACK - 1].var;

    int new_value = !ASSIGNMENT[var];
    unset_latest_assignment();
    set_literal(new_value ? var : -var, TRIED_BOTH_WAYS);

    return 1;
}

int main(int argc, char **argv) {
    LOG_XCHECK = argc > 1;
    for (char c; (c = getc(stdin)) == 'c';)
        while (getc(stdin) != '\n');

    assert(scanf(" cnf %u %u\n", &N_VARS, &N_CLAUSES) == 2);
    N_VARS++;

    ASSIGNMENT = malloc(N_VARS * sizeof(ASSIGNMENT[0]));
    memset(ASSIGNMENT, -1, N_VARS * sizeof(ASSIGNMENT[0]));

    DECISION_STACK = calloc(N_VARS, sizeof(DECISION_STACK[0]));

    CLAUSES = calloc(N_CLAUSES, sizeof(struct clause));

    LIT_TO_CLAUSES = calloc(N_VARS * 2, sizeof(LIT_TO_CLAUSES[0]));

    BCP_LIST = calloc(2 * N_VARS, sizeof(BCP_LIST[0]));
    IS_BCP_LISTED = calloc(2 * N_VARS, sizeof(IS_BCP_LISTED[0]));

    for (size_t i = 0; i < N_CLAUSES; i++) {
        int literal = 0;
        for (assert(scanf("%d ", &literal)); literal; assert(scanf("%d ", &literal))) {
            int repeat = 0;
            for (size_t j = 0; j < CLAUSES[i].n_literals && !repeat; j++)
                repeat = (CLAUSES[i].literals[j] == literal);
            if (repeat) continue;

            append_field(CLAUSES[i], literals) = literal;
        }
    }

    for (size_t i = 0; i < N_CLAUSES; i++) {
        if (!CLAUSES[i].n_literals) return unsatisfiable();
        if (CLAUSES[i].n_literals == 1) {
            queue_bcp(CLAUSES[i].literals[0]);
        } else {
            init_watcher(i, 0, CLAUSES[i].literals[0]);
            init_watcher(i, 1, CLAUSES[i].literals[1]);
        }
    }

    if (!bcp()) return unsatisfiable();
    while (1) {
        if (!decide())
            return satisfiable();

        while (!bcp())
            if (!resolveConflict())
                return unsatisfiable();
    }
}
