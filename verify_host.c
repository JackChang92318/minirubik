/* Host-side correctness gates for the IDA* solver, checked against the exact
 * BFS distance of every state:
 *   H1  max(permutation_pdb, orientation_pdb) never exceeds the true distance
 *   H2  both pattern databases are fully populated, solved entry 0
 *   H3  solve() returns a path of exactly the true distance for every state,
 *       and replaying that path reaches solved
 *
 * Usage: verify_host [--full | --sample STRIDE | --distance11 | --tables-only]
 * Default: full H1/H2 plus a sparse H3 smoke test, NOT full H3.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* Wall time, not CPU time. Windows is the measurement platform here. */
static double wall_seconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    if (!QueryPerformanceFrequency(&frequency) ||
        !QueryPerformanceCounter(&counter)) {
        fputs("FAIL: performance timer unavailable\n", stderr);
        exit(1);
    }
    return (double) counter.QuadPart / (double) frequency.QuadPart;
#else
    struct timespec t;
    if (timespec_get(&t, TIME_UTC) != TIME_UTC)
        exit(1);
    return (double) t.tv_sec + (double) t.tv_nsec / 1e9;
#endif
}

/* Reuse the solver under test rather than a copy of it: state_t, source,
 * twist, quarter_turn, apply_move, heuristic, is_solved and solve() all come
 * from here, together with the two pattern databases.
 */
#define VERIFY_HOST
#include "ida_solver.c"

enum {
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    MOVES = 9,
    MAX_DEPTH = 11,
    UNVISITED = UINT8_MAX
};

static const uint8_t inverse_move[MOVES] = {2, 1, 0, 5, 4, 3, 8, 7, 6};

/* Baseline move rules copied from solver.c, kept independent of IDA*. */
static const uint8_t oracle_source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};
static const uint8_t oracle_twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

/* The three quarter-turns preserve the fixed front-upper-left corner. */
static state_t oracle_quarter_turn(state_t state, uint8_t face)
{
    state_t result;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t from = oracle_source[face][i];
        result.p[i] = state.p[from];
        result.o[i] = (uint8_t) ((state.o[from] + oracle_twist[face][i]) % 3U);
    }
    return result;
}

static state_t oracle_apply_move(state_t state, uint8_t move)
{
    uint8_t turns = (uint8_t) (move % 3U + 1U);
    for (uint8_t i = 0; i < turns; ++i)
        state = oracle_quarter_turn(state, (uint8_t) (move / 3U));
    return state;
}


/* Same dense index as solver.c: rank / 729 is the Lehmer code of the
 * permutation, which matches unrank_to_rank() in ida_solver.c, and rank % 729
 * is o[0..5] in base 3, which matches rank_orientation().
 */
static uint32_t rank_state(const state_t *state)
{
    uint32_t p = 0, o = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t smaller = 0;
        for (uint8_t j = (uint8_t) (i + 1U); j < CUBIES; ++j)
            if (state->p[j] < state->p[i])
                ++smaller;
        p = p * (CUBIES - i) + smaller;
    }
    for (uint8_t i = 0; i < 6; ++i)
        o = o * 3U + state->o[i];
    return p * ORIENTATIONS + o;
}

static void unrank_state(uint32_t rank, state_t *state)
{
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    uint32_t p = rank / ORIENTATIONS, o = rank % ORIENTATIONS, f = 720;
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t q = (uint8_t) (p / f);
        p %= f;
        state->p[i] = available[q];
        for (uint8_t j = q; j + 1 < CUBIES - i; ++j)
            available[j] = available[j + 1U];
        if (i < 5)
            f /= 6U - i;
    }
    for (uint8_t i = 6; i-- > 0;) {
        state->o[i] = (uint8_t) (o % 3U);
        sum = (uint8_t) (sum + state->o[i]);
        o /= 3U;
    }
    state->o[6] = (uint8_t) ((3U - sum % 3U) % 3U);
}

static int valid(const state_t *state)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        if (state->p[i] >= CUBIES || state->o[i] >= 3)
            return 0;
        for (uint8_t j = 0; j < i; ++j)
            if (state->p[j] == state->p[i])
                return 0;
        sum = (uint8_t) (sum + state->o[i]);
    }
    return sum % 3U == 0;
}

/* Breadth-first search from solved over the whole space. On success returns
 * the move-toward-solved table and fills dist[rank] with the exact HTM
 * distance of every state; dist must hold STATES bytes.
 */
static uint8_t *build_table(uint8_t *dist, uint8_t *diameter)
{
    uint8_t *toward_solved = malloc(STATES);
    uint32_t *queue = malloc((size_t) STATES * sizeof *queue);
    uint16_t permutation[3][PERMUTATIONS], orientation[3][ORIENTATIONS];
    uint32_t head = 0, tail = 1, level_end = 1;
    state_t state;
    if (!toward_solved || !queue) {
        free(toward_solved);
        free(queue);
        return NULL;
    }
    for (uint16_t rank = 0; rank < PERMUTATIONS; ++rank) {
        unrank_state((uint32_t) rank * ORIENTATIONS, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = oracle_quarter_turn(state, face);
            permutation[face][rank] =
                (uint16_t) (rank_state(&next) / ORIENTATIONS);
        }
    }
    for (uint16_t rank = 0; rank < ORIENTATIONS; ++rank) {
        unrank_state(rank, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = oracle_quarter_turn(state, face);
            orientation[face][rank] =
                (uint16_t) (rank_state(&next) % ORIENTATIONS);
        }
    }
    memset(toward_solved, UNVISITED, STATES);
    memset(dist, UNVISITED, STATES);
    queue[0] = 0;
    toward_solved[0] = 0;
    dist[0] = 0;
    *diameter = 0;
    while (head < tail) {
        if (head == level_end) {
            level_end = tail;
            ++*diameter;
        }
        uint32_t here = queue[head++];
        uint16_t p = (uint16_t) (here / ORIENTATIONS);
        uint16_t o = (uint16_t) (here % ORIENTATIONS);
        for (uint8_t face = 0; face < 3; ++face) {
            uint16_t next_p = p, next_o = o;
            for (uint8_t turn = 0; turn < 3; ++turn) {
                next_p = permutation[face][next_p];
                next_o = orientation[face][next_o];
                uint32_t there = (uint32_t) next_p * ORIENTATIONS + next_o;
                if (toward_solved[there] == UNVISITED) {
                    uint8_t move = (uint8_t) (face * 3U + turn);
                    toward_solved[there] = inverse_move[move];
                    /* BFS reaches every state first along a shortest path. */
                    dist[there] = (uint8_t) (dist[here] + 1U);
                    queue[tail++] = there;
                }
            }
        }
    }
    free(queue);
    if (tail != STATES) {
        free(toward_solved);
        return NULL;
    }
    return toward_solved;
}

/* The dense index must be a bijection, or every per-rank check below would
 * be checking the wrong state.
 */
static int self_test(void)
{
    const state_t solved = {{0, 1, 2, 3, 4, 5, 6}, {0}};
    state_t state;
    for (uint8_t move = 0; move < MOVES; ++move) {
        state = apply_move(solved, move);
        state = apply_move(state, inverse_move[move]);
        if (memcmp(&solved, &state, sizeof solved))
            return 0;
    }
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        unrank_state(rank, &state);
        if (!valid(&state) || rank_state(&state) != rank ||
            is_solved(&state) != (rank == 0))
            return 0;
        uint16_t p;
        unrank_to_rank(state.p, &p);
        if (p != rank / ORIENTATIONS ||
            rank_orientation(state.o) != rank % ORIENTATIONS)
            return 0;
        for (uint8_t face = 0; face < 3; ++face) {
            state_t actual = quarter_turn(state, face);
            state_t expected = oracle_quarter_turn(state, face);
            if (memcmp(actual.p, expected.p, CUBIES) ||
                memcmp(actual.o, expected.o, CUBIES))
                return 0;
        }
    }
    return 1;
}

static uint8_t pdb_heuristic(uint32_t rank)
{
    uint8_t h_perm = permutation_pdb[rank / ORIENTATIONS];
    uint8_t h_ori = orientation_pdb[rank % ORIENTATIONS];
    return h_perm > h_ori ? h_perm : h_ori;
}

/* 14-digit input format of solver.c: cubies 1-7 then twists 1-3. */
static void format_state(const state_t *state, char out[15])
{
    for (int i = 0; i < CUBIES; ++i) {
        out[i] = (char) ('1' + state->p[i]);
        out[i + CUBIES] = (char) ('1' + state->o[i]);
    }
    out[14] = '\0';
}

static int check_bfs(const uint8_t *dist, uint8_t diameter)
{
    uint32_t histogram[MAX_DEPTH + 2] = {0};
    int failures = 0;
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        uint8_t d = dist[rank];
        ++histogram[d > MAX_DEPTH ? MAX_DEPTH + 1 : d];
    }
    printf("BFS: diameter %u\n", (unsigned) diameter);
    for (int d = 0; d <= MAX_DEPTH; ++d)
        printf("  depth %2d: %7lu\n", d, (unsigned long) histogram[d]);
    if (diameter != MAX_DEPTH || histogram[MAX_DEPTH + 1] != 0 ||
        histogram[0] != 1 || dist[0] != 0 || histogram[11] != 2644) {
        printf("FAIL: expected diameter %d with no deeper state\n", MAX_DEPTH);
        ++failures;
    }
    return failures;
}

/* H1: h(s) <= d(s) for every state s. */
static int check_h1(const uint8_t *dist)
{
    uint32_t violations = 0, first = 0;
    uint64_t sum_h = 0, sum_d = 0;
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        state_t state;
        unrank_state(rank, &state);
        uint8_t h = heuristic(&state);
        if (h != pdb_heuristic(rank)) {
            printf("H1 FAIL: actual heuristic/index mismatch at rank %lu\n",
                   (unsigned long) rank);
            return 1;
        }
        sum_h += h;
        sum_d += dist[rank];
        if (h > dist[rank] && violations++ == 0)
            first = rank;
    }
    printf("H1: mean h %.3f, mean distance %.3f\n", (double) sum_h / STATES,
           (double) sum_d / STATES);
    if (violations) {
        printf("H1 FAIL: %lu states overestimated, first rank %lu "
               "(h %u > d %u)\n",
               (unsigned long) violations, (unsigned long) first,
               (unsigned) pdb_heuristic(first), (unsigned) dist[first]);
        return 1;
    }
    printf("H1 PASS: heuristic admissible on all %d states\n", STATES);
    return 0;
}

/* Compare every PDB entry with the minimum exact full-state distance
 * among states with that projection. This also checks population/maxima. */
static int check_h2(const uint8_t *dist)
{
    uint8_t expected_p[PERMUTATIONS], expected_o[ORIENTATIONS];
    memset(expected_p, UNVISITED, sizeof expected_p);
    memset(expected_o, UNVISITED, sizeof expected_o);
    for (uint32_t r = 0; r < STATES; ++r) {
        unsigned p = r / ORIENTATIONS, o = r % ORIENTATIONS;
        if (dist[r] < expected_p[p]) expected_p[p] = dist[r];
        if (dist[r] < expected_o[o]) expected_o[o] = dist[r];
    }
    unsigned max_p = 0, max_o = 0;
    for (unsigned i = 0; i < PERMUTATIONS; ++i) {
        if (expected_p[i] == UNVISITED || permutation_pdb[i] != expected_p[i]) {
            printf("H2 FAIL: permutation entry %u\n", i);
            return 1;
        }
        if (permutation_pdb[i] > max_p) max_p = permutation_pdb[i];
    }
    for (unsigned i = 0; i < ORIENTATIONS; ++i) {
        if (expected_o[i] == UNVISITED || orientation_pdb[i] != expected_o[i]) {
            printf("H2 FAIL: orientation entry %u\n", i);
            return 1;
        }
        if (orientation_pdb[i] > max_o) max_o = orientation_pdb[i];
    }
    if (permutation_pdb[0] || orientation_pdb[0] || max_p != 7 || max_o != 6) {
        puts("H2 FAIL: solved entries or maxima");
        return 1;
    }
    printf("H2 PASS: all PDB entries match exact projected distances; "
           "maxima %u/%u, solved entries 0/0\n", max_p, max_o);
    return 0;
}

/* H3: solve() is optimal and its path is real, on every stride-th state. */
static int check_h3(const uint8_t *dist, uint32_t stride, int distance11_only)
{
    uint32_t checked = 0, wrong = 0;
    uint32_t worst_rank = 0;
    uint64_t total_generated = 0, worst_generated = 0;
    double start = wall_seconds();
    double last_progress = start;
    for (uint32_t rank = 0; rank < STATES; rank += stride) {
        if (distance11_only && dist[rank] != MAX_DEPTH)
            continue;
        state_t state, replay;
        uint8_t path[MAX_DEPTH];
        uint64_t generated = 0;
        unrank_state(rank, &state);
        int length = solve(state, path, &generated);
        int ok = length >= 0 && length <= MAX_DEPTH && length == dist[rank];
        if (ok) {
            replay = state;
            for (int i = 0; i < length; ++i) {
                if (path[i] >= MOVES) { ok = 0; break; }
                replay = oracle_apply_move(replay, path[i]);
            }
            ok = ok && valid(&replay) && rank_state(&replay) == 0;
        }
        if (!ok && wrong++ < 10)
            printf("H3 FAIL: rank %lu: length %d, distance %u\n",
                   (unsigned long) rank, length, (unsigned) dist[rank]);
        total_generated += generated;
        if (dist[rank] == MAX_DEPTH && generated > worst_generated) {
            worst_generated = generated;
            worst_rank = rank;
        }
        ++checked;
        double now = wall_seconds();
        if (now - last_progress >= 10.0) {
            printf("H3 progress: %lu checked; rank %lu/%d; %.1f wall seconds\n",
                    (unsigned long) checked, (unsigned long) rank, STATES, now - start);
            last_progress = now;
        }
        if (wrong) return 1;
    }
    double seconds = wall_seconds() - start;
    printf("H3: %lu states (stride %lu) in %.1f wall seconds, %llu child states generated\n",
           (unsigned long) checked, (unsigned long) stride, seconds,
           (unsigned long long) total_generated);
    if (worst_generated) {
        state_t worst;
        char text[15];
        unrank_state(worst_rank, &worst);
        format_state(&worst, text);
        printf("H3: hardest distance-11 state checked: %s (rank %lu), "
               "%llu nodes generated\n",
               text, (unsigned long) worst_rank,
               (unsigned long long) worst_generated);
    }
    if (wrong) {
        printf("H3 FAIL: %lu states not solved optimally\n",
               (unsigned long) wrong);
        return 1;
    }
    if (!distance11_only && stride == 1 && checked == STATES)
        puts("H3 FULL PASS: all states checked");
    else
        puts("H3 SUBSET PASS ONLY: full-domain H3 remains incomplete");
    return 0;
}

/* The vector every submission reports, 21345671111111. */
static int check_reference_vector(void)
{
    const state_t state = {{1, 0, 2, 3, 4, 5, 6}, {0}};
    uint8_t path[MAX_DEPTH];
    uint64_t generated = 0;
    int length = solve(state, path, &generated);
    printf("Reference 21345671111111: length %d, %llu nodes generated\n",
           length, (unsigned long long) generated);
    if (length != MAX_DEPTH) return 1;
    state_t replay = state;
    for (int i = 0; i < length; ++i) {
        if (path[i] >= MOVES) return 1;
        replay = oracle_apply_move(replay, path[i]);
    }
    return !valid(&replay) || rank_state(&replay) != 0;
}

int main(int argc, char **argv)
{
    uint32_t stride = 100000;
    int full = 0, tables_only = 0, distance11_only = 0;
    int failures = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc == 2 && !strcmp(argv[1], "--full")) {
        full = 1; stride = 1;
    } else if (argc == 2 && !strcmp(argv[1], "--tables-only")) {
        tables_only = 1;
    } else if (argc == 2 && !strcmp(argv[1], "--distance11")) {
        distance11_only = 1; stride = 1;
    } else if (argc == 3 && !strcmp(argv[1], "--sample")) {
        char *end;
        errno = 0;
        unsigned long n = strtoul(argv[2], &end, 10);
        if (errno || argv[2][0] < '0' || argv[2][0] > '9' || *end || n < 2 || n > STATES)
            goto usage;
        stride = (uint32_t) n;
    } else if (argc != 1) {
        goto usage;
    }
    printf("Mode: %s\n", full ? "FULL H3" : tables_only ? "H1/H2 only" :
           distance11_only ? "all distance-11 states (not full H3)" : "sample (not full H3)");
    puts("Checking full-domain encodings, goal predicate and baseline move agreement...");
    if (!self_test()) {
        printf("FAIL: rank/unrank or move inverse self-test\n");
        return 1;
    }
    puts("PASS: encoding and baseline quarter-turn agreement for all states");
    uint8_t *dist = malloc(STATES);
    uint8_t diameter;
    uint8_t *table = dist ? build_table(dist, &diameter) : NULL;
    if (!table) {
        fprintf(stderr, "FAIL: could not build baseline table\n");
        free(dist);
        return 1;
    }
    failures += check_bfs(dist, diameter);
    failures += check_h1(dist);
    failures += check_h2(dist);
    puts("H4: N/A -- PDB entries are unpacked uint8_t values");
    if (!failures && !tables_only) {
        failures += check_reference_vector();
        if (!failures) failures += check_h3(dist, stride, distance11_only);
    }
    if (failures) puts("FAILED");
    else if (full) puts("FULL HOST VERIFICATION PASS (H1/H2/H3; H4 N/A)");
    else puts("REQUESTED CHECKS PASSED; full H3 has NOT been completed by this run");
    free(table);
    free(dist);
    return failures ? 1 : 0;
usage:
    printf("usage: %s [--full | --tables-only | --distance11 | --sample STRIDE]\n", argv[0]);
    return 2;
}
