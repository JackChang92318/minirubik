#include <stdint.h>

#define PERM_COUNT 5040
#define CUBIES 7

static uint8_t pdb[PERM_COUNT];
static uint16_t queue[PERM_COUNT];

static const uint8_t source_R[7] = {1, 4, 2, 0, 3, 5, 6};
static const uint8_t source_B[7] = {0, 1, 2, 4, 5, 6, 3};
static const uint8_t source_D[7] = {0, 2, 5, 3, 1, 4, 6};

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

int main(){
    unsigned head = 0;
    unsigned tail = 0;

    for (unsigned i = 0; i < PERM_COUNT; ++i) {
        pdb[i] = 255;
    }

    pdb[0] = 0;
    queue[tail] = 0;
    tail = tail + 1;
    while (head < tail) {
        uint16_t current_rank = queue[head];
        head = head + 1;

        uint8_t current_perm[7] ;
        rank_to_unrank(current_rank, current_perm);

        uint8_t next_perm[7];

        for (int i = 0; i < 7; ++i) {
            next_perm[i] = current_perm[source_R[i]];
        }
    }

    return 0;
}