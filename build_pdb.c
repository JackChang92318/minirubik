#include <stdint.h>
#include <stdio.h>

#define PERM_COUNT 5040
#define CUBIES 7

static uint8_t pdb[PERM_COUNT];
static uint16_t queue[PERM_COUNT];

static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};

void rank_to_unrank(uint16_t rank, uint8_t *perm) {
    uint8_t available[7] = {0, 1, 2, 3, 4, 5, 6};
    for (int i = 0; i < 7; ++i) {
        uint16_t fact = 1;
        for (int j = 1; j <= 6 - i; ++j) {
            fact *= j;
        }
        uint16_t index = rank / fact;
        perm[i] = available[index];
        for (int j = index; j < 6 - i; ++j) {
            available[j] = available[j + 1];
        }
        rank %= fact;
    }
}

void unrank_to_rank(const uint8_t *perm, uint16_t *rank) {
    uint8_t available[7] = {0, 1, 2, 3, 4, 5, 6};
    *rank = 0;
    for (int i = 0; i < 7; i++) {
        uint16_t index = 0;
        while (available[index] != perm[i]) {
            index++;
        }
        for (int j = index; j < 6 - i; ++j) {
            available[j] = available[j + 1];
        }
        uint16_t fact = 1;
        for (int j = 1; j <= 6 - i; ++j) {
            fact *= j;
        }
        *rank += index * fact;
    }
}

int main(){
    unsigned head = 0;
    unsigned tail = 0;

    for (unsigned i = 0; i < PERM_COUNT; i++) {
        pdb[i] = 255;
    }

    pdb[0] = 0;
    queue[tail] = 0;
    tail = tail + 1;
    while (head < tail) {
        uint16_t current_rank = queue[head];
        head = head + 1;

        uint8_t current_perm[CUBIES]; ;
        rank_to_unrank(current_rank, current_perm);

        for (int face = 0; face < 3; face++) {
            uint8_t working_perm[CUBIES];
            uint8_t next_perm[CUBIES];

            for (int i = 0; i < CUBIES; i++) {
                working_perm[i] = current_perm[i];
            }

            for (int turns = 1; turns <= 3; ++turns) {
                for (int i = 0; i < CUBIES; i++) {
                    next_perm[i] = working_perm[source[face][i]];
                }

                uint16_t next_rank;
                unrank_to_rank(next_perm, &next_rank);

                if (pdb[next_rank] == 255) {
                    pdb[next_rank] = pdb[current_rank] + 1;
                    queue[tail] = next_rank;
                    tail = tail + 1;
                }

                for (int i = 0; i < CUBIES; i++) {
                    working_perm[i] = next_perm[i];
                }
            }
        }
    }

    FILE *out = fopen("include/pdb_data.h", "w");

    fprintf(out, "#ifndef PDB_DATA_H\n");
    fprintf(out, "#define PDB_DATA_H\n\n");
    fprintf(out, "#include <stdint.h>\n\n");
    fprintf(out, "static const uint8_t permutation_pdb[5040] = {\n");

    for (unsigned i = 0; i < PERM_COUNT; ++i) {
        fprintf(out, "%u,", (unsigned) pdb[i]);

        if ((i + 1) % 16 == 0) {
            fprintf(out, "\n");
        } else {
            fprintf(out, " ");
        }
    }

    fprintf(out, "\n};\n\n#endif\n");

    if (fclose(out) != 0) {
        perror("Cannot finish writing pdb_data.h");
        return 1;
    }

    printf("Created pdb_data.h\n");

    FILE *asm_out = fopen("pdb_data.s", "w");
    if (asm_out == NULL) {
        perror("Cannot open pdb_data.s");
        return 1;
    }

    fprintf(asm_out, ".section .rodata\n");
    fprintf(asm_out, "permutation_pdb:\n");

    for (unsigned i = 0; i < PERM_COUNT; ++i) {
        if (i % 16 == 0) {
            fprintf(asm_out, "    .byte ");
        }

        fprintf(asm_out, "%u", (unsigned) pdb[i]);

        if (i % 16 == 15 || i + 1 == PERM_COUNT) {
            fprintf(asm_out, "\n");
        } else {
            fprintf(asm_out, ", ");
        }
    }

    if (fclose(asm_out) != 0) {
        perror("Cannot finish writing pdb_data.s");
        return 1;
    }

    printf("Created pdb_data.s\n");

    return 0;
}