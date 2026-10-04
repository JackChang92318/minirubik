#include <stdint.h>
#include <stdio.h>

#define ORI_COUNT 729
#define CUBIES 7

static uint8_t pdb[ORI_COUNT];
static uint16_t queue[ORI_COUNT];

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


static uint16_t rank_orientation(const uint8_t *o){
    uint16_t rank = 0;

    for (int i = 0; i < 6; i++) {
        rank = rank * 3 + o[i];
    }

    return rank;
}

static void unrank_to_orientation(uint16_t rank, uint8_t *o){
    uint8_t sum = 0;

    for (int i = 5; i >= 0; i--) {
        o[i] = (uint8_t) (rank % 3);
        sum = (uint8_t) (sum + o[i]);
        rank /= 3;
    }
    o[6] = (uint8_t) ((3 - sum % 3) % 3);
}

/* Check shortest-distance properties on every abstract edge. */
static int check_distances(void)
{
    unsigned edges = 0;
    for (uint16_t r = 0; r < ORI_COUNT; ++r) {
        uint8_t current[CUBIES];
        unrank_to_orientation(r, current);
        int found_closer = 0;

        for (int face = 0; face < 3; ++face) {
            uint8_t working[CUBIES];
            for (int i = 0; i < CUBIES; ++i)
                working[i] = current[i];

            for (int turns = 1; turns <= 4; ++turns) {
                uint8_t next[CUBIES];
                unsigned sum = 0;
                for (int i = 0; i < CUBIES; ++i) {
                    next[i] = (uint8_t)
                        ((working[source[face][i]] + twist[face][i]) % 3);
                    sum += next[i];
                }
                if (sum % 3 != 0) {
                    printf("FAIL: orientation invariant at rank %u\n", (unsigned) r);
                    return 0;
                }

                if (turns == 4) {
                    for (int i = 0; i < CUBIES; ++i) {
                        if (next[i] != current[i]) {
                            printf("FAIL: four turns at rank %u face %d\n",
                                   (unsigned) r, face);
                            return 0;
                        }
                    }
                } else {
                    uint16_t neighbor = rank_orientation(next);
                    int difference = (int) pdb[neighbor] - (int) pdb[r];
                    if (difference < -1 || difference > 1) {
                        printf("FAIL: distance edge %u -> %u\n",
                               (unsigned) r, (unsigned) neighbor);
                        return 0;
                    }
                    if (difference == -1)
                        found_closer = 1;
                    ++edges;
                }
                for (int i = 0; i < CUBIES; ++i)
                    working[i] = next[i];
            }
        }
        if (r != 0 && !found_closer) {
            printf("FAIL: no closer neighbor at rank %u\n", (unsigned) r);
            return 0;
        }
    }
    printf("PASS: %u neighbor distance checks; all non-goals have a closer neighbor\n", edges);
    printf("PASS: orientation invariants and four-turn restoration\n");
    return 1;
}

int main(void){

    for (uint16_t r = 0; r < ORI_COUNT; ++r) {
        uint8_t o[CUBIES];
        unrank_to_orientation(r, o);

        unsigned sum = 0;
        for (int i = 0; i < CUBIES; ++i) {
            if (o[i] > 2) {
                printf("FAIL: invalid orientation at rank %u\n",
                    (unsigned) r);
                return 1;
            }
            sum += o[i];
        }

        if (rank_orientation(o) != r || sum % 3 != 0) {
            printf("FAIL: orientation conversion at rank %u\n",
                (unsigned) r);
            return 1;
        }
    }
    printf("PASS: all 729 orientation conversions\n");


    unsigned head = 0;
    unsigned tail = 0;

    for (unsigned i = 0; i < ORI_COUNT; i++) {
        pdb[i] = 255;
    }

    pdb[0] = 0;
    queue[tail] = 0;
    tail = tail + 1;
    while (head < tail) {
        uint16_t current_rank = queue[head];
        head = head + 1;

        uint8_t current_ori[CUBIES]; 
        unrank_to_orientation(current_rank, current_ori);

        for (int face = 0; face < 3; face++) {
            uint8_t working_ori[CUBIES];
            uint8_t next_ori[CUBIES];

            for (int i = 0; i < CUBIES; i++) {
                working_ori[i] = current_ori[i];
            }

            for (int turns = 1; turns <= 3; ++turns) {
                for (int i = 0; i < CUBIES; i++) {
                    next_ori[i] = (working_ori[source[face][i]] + twist[face][i]) % 3;
                }

                uint16_t next_rank = rank_orientation(next_ori);

                if (pdb[next_rank] == 255) {
                    pdb[next_rank] = pdb[current_rank] + 1;
                    queue[tail] = next_rank;
                    tail = tail + 1;
                }

                for (int i = 0; i < CUBIES; i++) {
                    working_ori[i] = next_ori[i];
                }
            }
        }
    }

    unsigned missing = 0;
    unsigned zeros = 0;
    unsigned maximum = 0;

    for (unsigned i = 0; i < ORI_COUNT; ++i) {
        if (pdb[i] == 255) {
            ++missing;
            continue;
        }
        if (pdb[i] == 0)
            ++zeros;
        if (pdb[i] > maximum)
            maximum = pdb[i];
    }

    printf("Visited: %u\n", tail);
    printf("Missing: %u\n", missing);
    printf("Zero entries: %u\n", zeros);
    printf("Maximum distance: %u\n", maximum);

    if (tail != ORI_COUNT || missing != 0 ||
        zeros != 1 || pdb[0] != 0) {
        printf("FAIL: table coverage\n");
        return 1;
    }

    if (!check_distances())
        return 1;

    FILE *out = fopen("pdb_data_ori.h", "w");
    if (out == NULL) {
        perror("Cannot create pdb_data_ori.h");
        return 1;
    }

    fprintf(out, "#ifndef PDB_DATA_ORI_H\n");
    fprintf(out, "#define PDB_DATA_ORI_H\n\n");
    fprintf(out, "#include <stdint.h>\n\n");
    fprintf(out, "static const uint8_t orientation_pdb[729] = {\n");

    for (unsigned i = 0; i < ORI_COUNT; ++i) {
        fprintf(out, "%u,", (unsigned) pdb[i]);

        if ((i + 1) % 16 == 0) {
            fprintf(out, "\n");
        } else {
            fprintf(out, " ");
        }
    }

    fprintf(out, "\n};\n\n#endif\n");

    if (fclose(out) != 0) {
        perror("Cannot finish writing pdb_data_ori.h");
        return 1;
    }

    printf("Created pdb_data_ori.h\n");

    return 0;
}
