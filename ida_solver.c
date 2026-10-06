#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include <string.h>
#include "pdb_data.h"
#include "pdb_data_ori.h"

#define CUBIES 7

typedef struct {
    uint8_t p[CUBIES];
    uint8_t o[CUBIES];
} state_t;

static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};
static const uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

void unrank_to_rank(const uint8_t *perm, uint16_t *rank) {
    uint8_t available[7] = {0, 1, 2, 3, 4, 5, 6};
    *rank = 0;
    for(int i = 0; i < 7; i++){
        uint16_t index = 0;
        while(available[index] != perm[i]){
            index++;
        }
        for(int j = index; j < 6 - i; ++j){
            available[j] = available[j + 1];
        }
        uint16_t fact = 1;
        for(int j = 1; j <= 6 - i; ++j){
            fact *= j;
        }
        *rank += index * fact;
    }
}


static uint16_t rank_orientation(const uint8_t *o){
    uint16_t rank = 0;

    for(int i = 0; i < 6; i++){
        rank = rank * 3 + o[i];
    }

    return rank;
}

static uint8_t heuristic(const state_t *state){
    uint16_t rank;
    unrank_to_rank(state->p, &rank);

    uint8_t h_perm = permutation_pdb[rank];
    uint8_t h_ori = orientation_pdb[rank_orientation(state->o)];

    if(h_perm > h_ori){
        return h_perm;
    }
    return h_ori;
}

static int is_solved(const state_t *state){
    for(int i = 0; i < CUBIES; i++){
        if (state->p[i] != i || state->o[i] != 0) {
            return 0;
        }
    }
    return 1;
}

static state_t quarter_turn(state_t state, uint8_t face){
    state_t result;

    for(uint8_t i = 0; i < CUBIES; i++){
        uint8_t from = source[face][i];
        result.p[i] = state.p[from];
        result.o[i] = (uint8_t)((state.o[from] + twist[face][i]) % 3U);
    }
    return result;
}

static state_t apply_move(state_t state, uint8_t move){
    uint8_t turns = (uint8_t) (move % 3U + 1U);
    for(uint8_t i = 0; i < turns; ++i){
        state = quarter_turn(state, (uint8_t) (move / 3U));
    }
    return state;
}

static int solve(state_t start, uint8_t path[11], uint64_t *generated_out){
    state_t states[12];

    uint8_t next_moves[12];

    int depth = 0;

    states[0] = start;
    next_moves[0] = 0;

    int bound = heuristic(&states[0]);
    int found = 0;
    uint64_t generated = 0;

    while(1){
        depth = 0;
        next_moves[0] = 0;
        int next_bound = INT_MAX;
        while(1){
            int f = depth + heuristic(&states[depth]);

            if(f > bound){
                if(f < next_bound){
                    next_bound = f;
                }

                if(depth == 0){
                    break;
                } else {
                    depth--;
                    continue;
                }
            }
            if(is_solved(&states[depth])){
                found = 1;
                break;
            }
            if(next_moves[depth] >= 9 || depth >= 11){
                if(depth == 0){
                    break;
                } else {
                    depth--;
                    continue;
                }
            }
            uint8_t move = next_moves[depth];
            next_moves[depth]++;
            if(depth > 0 && move / 3 == path[depth - 1] / 3){
                continue;
            }
            path[depth] = move;
            states[depth + 1] = apply_move(states[depth], move);
            generated++;
            depth++;
            next_moves[depth] = 0;
        }
        if(found){
            break;
        }

        if(next_bound == INT_MAX || next_bound > 11){
            break;
        }

        bound = next_bound;
    }

    if (generated_out != NULL)
        *generated_out = generated;

    if (found) {
        return depth;
    }
    return -1;
}
static int parse_state(const char *input, state_t *state)
{
    if (strlen(input) != 14)
        return 0;

    unsigned seen = 0;
    unsigned orientation_sum = 0;

    for (int i = 0; i < CUBIES; ++i) {
        if (input[i] < '1' || input[i] > '7')
            return 0;

        uint8_t cubie = (uint8_t) (input[i] - '1');
        unsigned bit = 1U << cubie;

        if (seen & bit)
            return 0;

        seen |= bit;
        state->p[i] = cubie;

        if (input[i + CUBIES] < '1' ||
            input[i + CUBIES] > '3')
            return 0;

        state->o[i] =
            (uint8_t) (input[i + CUBIES] - '1');
        orientation_sum += state->o[i];
    }

    return orientation_sum % 3U == 0;
}

int main(int argc, char **argv){
    state_t start;

    if(argc != 2 || !parse_state(argv[1], &start)){
        fprintf(stderr,
                "Usage: %s PPPPPPPOOOOOOO\n"
                "P: digits 1-7, each appearing once.\n"
                "O: digits 1-3; decoded orientations "
                "must sum to a multiple of 3.\n",
                argc > 0 && argv[0] ? argv[0] : "ida_solver");
        return 2;
    }

    uint8_t path[11];
    int length = solve(start, path, NULL);

    if(length < 0){
        fputs("No solution found within 11 moves\n", stderr);
        return 1;
    }

    static const char *const move_names[9] = {
        "R", "R2", "R'",
        "B", "B2", "B'",
        "D", "D2", "D'"
    };

    for(int i = 0; i < length; i++)
        printf("%s%s", i == 0 ? "" : " ", move_names[path[i]]);

    putchar('\n');

    return fflush(stdout) != 0 || ferror(stdout) ? 1 : 0;
}