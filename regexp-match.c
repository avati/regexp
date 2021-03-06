/*
 * Credits: neeldhara@imsc.res.in
 * Bugs   : avati@gluster.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* define this when all constructs are E-loop free */
#define MISERIES_IN_LIFE_ARE_SOLVED
#define DEPTH_OF_MISERY 43

typedef enum {
        SYMBOL,          /* alphabet, '.', etc */
        OP_CLOSURE,      /* '*' */
        OP_START_UNION,  /* '[' */
        OP_STOP_UNION,   /* ']' */
        OP_START_CONCAT, /* '(' */
        OP_STOP_CONCAT,  /* ')' */
} element_type_t;


struct transition;
struct state;


struct transition {
        struct transition *next;
#define E 0
        char label;                  /* 0 label = E transition */
        struct state *to;
};


struct state {
        struct state       *next;       /* list of all related states */
        struct state       *prev;

        char               ch;
        int                id;          /* Unique identifier, per state */
        int                is_start;    /* 0 = false, 1 = true */
        int                is_final;    /* 0 = false, 1 = true */
        int                T;           /* this was added by a closure */
        struct transition *transitions; /* list of transitions from here */
};


element_type_t
element_type (char element)
{
        element_type_t type = SYMBOL;

        switch (element) {
        case '*': type = OP_CLOSURE; break;
        case '(': type = OP_START_CONCAT; break;
        case ')': type = OP_STOP_CONCAT; break;
        case '[': type = OP_START_UNION; break;
        case ']': type = OP_STOP_UNION; break;
        }

        return type;
}


int
state_foreach (struct state *start, int (*func) (struct state *each,
                                                 void *data),
               void *data)
{
        struct state *trav = NULL;
        int           ret = 0;

        trav = start;
        do {
                ret = func (trav, data);
                if (ret)
                        break;
                trav = trav->next;
        } while (trav != start);

        return ret;
}


int
transition_foreach (struct state *state, int (*func) (struct state *state,
                                                      struct transition *each,
                                                      void *data),
                    void *data)
{
        struct transition *trav = NULL;
        int                ret = 0;

        trav = state->transitions;

        while (trav) {
                ret = func (state, trav, data);
                if (ret)
                        break;
                trav = trav->next;
        }

        return ret;
}


int
state_splice (struct state *one, struct state *two)
{
        struct state *head1 = NULL;
        struct state *tail1 = NULL;
        struct state *head2 = NULL;
        struct state *tail2 = NULL;

        head1 = one;
        tail1 = one->prev;

        head2 = two;
        tail2 = two->prev;

        tail1->next = head2;
        head2->prev = tail1;

        head1->prev = tail2;
        tail2->next = head1;

        return 0;
}


struct state *
new_state (int id)
{
        struct state *newstate = NULL;

        newstate = calloc (1, sizeof (*newstate));
        newstate->id = id + 1;

        newstate->next = newstate;
        newstate->prev = newstate;

        return newstate;
}


struct state *
new_start_state (int id)
{
        struct state *newstate = NULL;

        newstate = new_state (id);
        newstate->is_start = 1;

        return newstate;
}

struct state *
new_final_state (int id)
{
        struct state *newstate = NULL;

        newstate = new_state (id);
        newstate->is_final = 1;

        return newstate;
}


struct transition *
new_transition (char label)
{
        struct transition *newtrans = NULL;

        newtrans = calloc (1, sizeof (*newtrans));
        newtrans->label = label;

        return newtrans;
}


int
state_transition (struct state *from, struct state *to, char label)
{
        struct transition *newtrans = NULL;

        newtrans = new_transition (label);

        newtrans->to = to;

        newtrans->next = from->transitions;
        from->transitions = newtrans;

        return 0;
}


struct state *
add_symbol (struct state *start, char label, int idx)
{
        struct state *symstate = NULL;

        symstate = new_final_state (idx);
        symstate->ch = start->ch;

        state_transition (start, symstate, label);

        state_splice (start, symstate);

        return start;
}


int
E_transition_if_final (struct state *each, void *data)
{
        struct state *newstate = NULL;

        newstate = data;

        if (each->is_final) {
                state_transition (each, newstate, E);
        }

        return 0;
}


int
unfinalize (struct state *each, void *data)
{
        struct state *newstate = NULL;

        newstate = data;

        if (each->is_final) {
                each->is_final = 0;
        }

        return 0;
}


int
E_transition_if_final_and_not_T (struct state *each, void *data)
{
        struct state *newstate = NULL;

        newstate = data;

        if (each->is_final && !each->T) {
                state_transition (each, newstate, E);
        }

        return 0;
}


struct state *
add_closure (struct state *newstate, struct state *prev)
{
        state_foreach (prev, E_transition_if_final_and_not_T, newstate);

        newstate->is_final = 1;
        prev->is_start = 0;

        state_transition (newstate, prev, E);

        state_splice (newstate, prev);

        return newstate;
}


struct state *
add_union (struct state *newstate, struct state *subex)
{
        state_transition (newstate, subex, E);

        state_splice (newstate, subex);

        subex->is_start = 0;

        return newstate;
}


struct state *
add_concat (struct state *left, struct state *right)
{
        if (!left)
                return right;

        state_foreach (left, E_transition_if_final, right);

        state_foreach (left, unfinalize, NULL);

        right->is_start = 0;

        state_splice (left, right);

        return left;
}


struct state *
next_subex (const char *regex, int *idx, struct state *prev)
{
        struct state *newstate = NULL;
        struct state *subex = NULL;
        struct state *ret = NULL;

        switch (element_type (regex[*idx])) {

        case SYMBOL:
                newstate = new_start_state (*idx);
                newstate->ch = regex[*idx];

                add_symbol (newstate, regex[*idx], (*idx));
                ret = newstate;
                break;

        case OP_CLOSURE:
                newstate = new_start_state (*idx);
                newstate->ch = regex[*idx];
                newstate->T = 1;

                if (!prev) {
                        fprintf (stderr,
                                 "RegExp is not balanced. unexpected *\n");
                        return NULL;
                }

                /* since closure is a post-op in RegExp syntax,
                   the manipulated node is really @prev
                */
                add_closure (newstate, prev);
                ret = newstate;
                break;

        case OP_START_UNION:
                newstate = new_start_state (*idx);
                newstate->ch = regex[*idx];

                (*idx)++;

                while (regex[*idx]) {
                        if (element_type (regex[*idx]) == OP_STOP_UNION)
                                break;

                        subex = next_subex (regex, idx, subex);

                        if (!subex)
                                return NULL;

                        add_union (newstate, subex);
                        subex = newstate;
                        (*idx)++;
                }

                if (element_type (regex[*idx]) != OP_STOP_UNION) {
                        fprintf (stderr,
                                 "RegExp is not balanced with a closing ]\n");
                        return NULL;
                }

                ret = newstate;
                break;

        case OP_START_CONCAT:
                (*idx)++;
                while (regex[*idx]) {
                        if (element_type (regex[*idx]) == OP_STOP_CONCAT)
                                break;

                        subex = next_subex (regex, idx, subex);

                        if (!subex)
                                return NULL;

                        newstate = add_concat (newstate, subex);
                        subex = newstate;
                        (*idx)++;
                }

                if (element_type (regex[*idx]) != OP_STOP_CONCAT) {
                        fprintf (stderr, "RegExp is not balanced with a closing )\n");
                        return NULL;
                }

                ret = newstate;
                break;

        case OP_STOP_CONCAT:
                fprintf (stderr, "RegExp is not balanced ... unexpected ')'\n");
                return NULL;

        case OP_STOP_UNION:
                fprintf (stderr, "RegExp is not balanced ... unexpected ']'\n");
                return NULL;
        }

        /* peek ahead */
        if (element_type (regex[(*idx)+1]) == OP_CLOSURE) {
                (*idx)++;
                ret = next_subex (regex, idx, ret);
        }

        return ret;
}


int
parse_regex (struct state *start, const char *regex)
{
        int idx = 0;
        struct state *subex = NULL;

        while (regex[idx]) {
                subex = next_subex (regex, &idx, start);
                if (!subex)
                        return -1;
                add_concat (start, subex);
                idx++;
        }

        return 0;
}


int
match_regex (struct state *state, const char *input);

int
attempt_E_move (struct state *state, struct transition *each,
                void *data)
{
        char *input = NULL;
        int   ret = 0;

        input = data;

        if (each->label == E) {
                ret = match_regex (each->to, input);
        }

        return ret;
}


int
attempt_nonE_move (struct state *state, struct transition *each,
                   void *data)
{
        char *input = NULL;
        int   ret = 0;

        input = data;

        if ((each->label == (*input)) || (each->label == '.')) {
                ret = match_regex (each->to, input+1);
        }

        return ret;
}


int depth = 0;

int
match_regex (struct state *state, const char *input)
{
        int ret = 0;

#ifndef MISERIES_IN_LIFE_ARE_SOLVED
        if (!depth)
                return 0;
#endif

        depth--;
        {
                /* check for E moves before checking end of input */
                ret = transition_foreach (state, attempt_E_move, (void *)input);
        }
        depth++;

        if (ret) /* save trouble of finding more ways to accept
                    if one is already found
                 */
                return ret;

        if ((*input) == 0) { /* end of input */

                if (state->is_final) { /* current state is a final state */
                        return 1; /* accept */
                }
                return 0; /* nope */
        }

        ret = transition_foreach (state, attempt_nonE_move, (void *)input);

        return ret;
}


int
main (int argc, char *argv[])
{
        char *regex = NULL;
        char *input = NULL;

        /* Hardcoded start state, to kick-start */
        struct state start = {
                .next        = &start,
                .prev        = &start,
                .id          = 0,
                .is_start    = 1,
                .is_final    = 1,
                .transitions = NULL,
        };

        if (argc != 3) {
                fprintf (stderr, "Usage: %s <regex> <input>\n",
                         argv[0]);
                return 1;
        }

        regex = argv[1];
        input = argv[2];

        if (parse_regex (&start, regex) != 0) {
                return 1;
        }

        depth = DEPTH_OF_MISERY;
        if (match_regex (&start, input) == 1) {
                printf ("%s accepts %s\n", regex, input);
        } else {
                printf ("%s does not accept %s\n", regex, input);
        }

        return 0;
}
