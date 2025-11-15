#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define LONG_URL_MAX 2048
#define SHORT_CODE_LEN 7
#define HASH_SIZE 1007

static const char *BASE62 = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

// Scrambling configuration to avoid linear short codes
#define MODULUS (67108864ULL)          // 2^26 space
#define PRIME_MULTIPLIER (36779219ULL) // Odd multiplier ensures permutation mod 2^n

// Scramble a sequential id into a pseudorandom-looking id in [0, MODULUS)
static uint64_t scramble_id(uint64_t sequential_id) {
    if (sequential_id >= MODULUS) {
        return 0;
    }
    uint64_t scrambled_id = (sequential_id * PRIME_MULTIPLIER) % MODULUS;
    return scrambled_id;
}

// node (short -> long)
typedef struct ShortNode {
    char short_code[SHORT_CODE_LEN + 1];
    char *long_url;
    struct ShortNode *next;
} ShortNode;

// node (long -> short)
typedef struct LongNode {
    char *long_url;
    char short_code[SHORT_CODE_LEN + 1];
    struct LongNode *next;
} LongNode;

ShortNode *short_table[HASH_SIZE];
LongNode *long_table[HASH_SIZE];

//global counter for generating unique IDs
uint64_t global_id = 1;


//standard djb2 hashing function
unsigned long hash_str(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++))
        hash = ((hash << 5) + hash) + c;
    return hash % HASH_SIZE;
}

// encode integer id to base62 fixed-length short code
void id_to_base62(uint64_t id, char *out) {
    char buf[SHORT_CODE_LEN + 1];
    buf[SHORT_CODE_LEN] = '\0';
    for (int i = SHORT_CODE_LEN - 1; i >= 0; --i) {
        buf[i] = BASE62[id % 62];
        id /= 62;
    }
    strcpy(out, buf);
}


// find in short table
ShortNode *find_short(const char *short_code) {
    unsigned long h = hash_str(short_code);
    ShortNode *cur = short_table[h];
    while (cur) {
        if (strcmp(cur->short_code, short_code) == 0) return cur;
        cur = cur->next;
    }
    return NULL;
}

// find in long table
LongNode *find_long(const char *long_url) {
    unsigned long h = hash_str(long_url);
    LongNode *cur = long_table[h];
    while (cur) {
        if (strcmp(cur->long_url, long_url) == 0) return cur;
        cur = cur->next;
    }
    return NULL;
}

// insert to short table
void insert_short(const char *short_code, const char *long_url) {
    unsigned long h = hash_str(short_code);
    ShortNode *node = malloc(sizeof(ShortNode));
    strcpy(node->short_code, short_code);
    node->long_url = strdup(long_url);
    node->next = short_table[h];
    short_table[h] = node;
}

// insert to long table
void insert_long(const char *long_url, const char *short_code) {
    unsigned long h = hash_str(long_url);
    LongNode *node = malloc(sizeof(LongNode));
    node->long_url = strdup(long_url);
    strcpy(node->short_code, short_code);
    node->next = long_table[h];
    long_table[h] = node;
}

// remove from short_table. returns 1 if removed
int remove_short(const char *short_code) {
    unsigned long h = hash_str(short_code);
    ShortNode *cur = short_table[h];
    ShortNode *prev = NULL;
    while (cur) {
        if (strcmp(cur->short_code, short_code) == 0) {
            if (prev) prev->next = cur->next;
            else short_table[h] = cur->next;
            free(cur->long_url);
            free(cur);
            return 1;
        }
        prev = cur;
        cur = cur->next;
    }
    return 0;
}

// Remove from Long table, returns 1 if removed
int remove_long(const char *long_url) {
    unsigned long h = hash_str(long_url);
    LongNode *cur = long_table[h];
    LongNode *prev = NULL;
    while (cur) {
        if (strcmp(cur->long_url, long_url) == 0) {
            if (prev) prev->next = cur->next;
            else long_table[h] = cur->next;
            free(cur->long_url);
            free(cur);
            return 1;
        }
        prev = cur;
        cur = cur->next;
    }
    return 0;
}

// Generate short URL. If long URL already present, return existing short code.
void generate_short_url(const char *long_url, char *out_short_code) {
    // check if this long URL already has a short code
    LongNode *existing = find_long(long_url);
    if (existing) {
        strcpy(out_short_code, existing->short_code);
        return;
    }

    // attempt to create a new unique short code using global_id
    for (;;) {
        char candidate[SHORT_CODE_LEN + 1];
        // scramble the sequential id within a bounded range to avoid linear patterns
        uint64_t seq = global_id % MODULUS;
        uint64_t scrambled = scramble_id(seq);
        id_to_base62(scrambled, candidate);

        // if short code unused, insert mapping
        if (!find_short(candidate)) {
            insert_short(candidate, long_url);
            insert_long(long_url, candidate);
            strcpy(out_short_code, candidate);
            global_id++;
            return;
        }
        // conflict: increment id and try again
        global_id++;

    }
}

// Retrieve original long URL given short code. Returns 1 if found and fills buffer.
int retrieve_original(const char *short_code, char *out_long_url, size_t out_size) {
    ShortNode *n = find_short(short_code);
    if (!n) return 0;
    strncpy(out_long_url, n->long_url, out_size - 1);
    out_long_url[out_size - 1] = '\0';
    return 1;
}

// Delete mapping given short code. Returns 1 on success.
int delete_short(const char *short_code) {
    ShortNode *n = find_short(short_code);
    if (!n) return 0;
    // get the long URL to remove reverse mapping
    char long_url[LONG_URL_MAX];
    strncpy(long_url, n->long_url, sizeof(long_url) - 1);
    long_url[sizeof(long_url) - 1] = '\0';

    int removed1 = remove_short(short_code);
    int removed2 = remove_long(long_url);
    return removed1 && removed2;
}

//print all mappings (for debug)
void print_all_mappings() {
    printf("Current mappings (short -> long):\n");
    for (int i = 0; i < HASH_SIZE; ++i) {
        ShortNode *cur = short_table[i];
        while (cur) {
            printf("%s -> %s\n", cur->short_code, cur->long_url);
            cur = cur->next;
        }
    }
}

// free memory
void cleanup_all() {
    for (int i = 0; i < HASH_SIZE; ++i) {
        ShortNode *s = short_table[i];
        while (s) {
            ShortNode *t = s->next;
            free(s->long_url);
            free(s);
            s = t;
        }
        short_table[i] = NULL;

        LongNode *l = long_table[i];
        while (l) {
            LongNode *t = l->next;
            free(l->long_url);
            free(l);
            l = t;
        }
        long_table[i] = NULL;
    }
    printf("Clean-Up Done!!\nExiting Code...\n");
}

void count(){
    int short_count, long_count; short_count = long_count = 0; 
    for (int i = 0; i < HASH_SIZE; i++)
    {
        if (short_table[i])
        {
            short_count++;
        }
        if (long_table[i])
        {
            long_count++;   
        }
    }
    printf("Short_table count->%d\nLong_table count->%d\n", short_count, long_count);
}

int main() {
    char cmd[16];
    char buffer[LONG_URL_MAX];
    char short_code[SHORT_CODE_LEN + 1];

    printf("URL Shortener CLI\n");
    printf("Commands: gen <long_url>, get <short_code>, del <short_code>, list, exit\n");

    while (1) {
        printf("> ");
        if (!fgets(buffer, sizeof(buffer), stdin)) break;
        buffer[strcspn(buffer, "\n")] = 0;
        if (strlen(buffer) == 0) continue;

        if (sscanf(buffer, "%15s", cmd) != 1) continue;

        if (strcmp(cmd, "gen") == 0) {
            // long url starts after command + space
            char *p = buffer + 3;
            while (*p == ' ') p++;
            if (*p == '\0') {
                printf("Usage: gen <long_url>\n");
                continue;
            }
            // Check if URL is too long
            if (strlen(p) >= LONG_URL_MAX) {
                printf("Error: URL is too long! Maximum allowed length is %d characters.\n", LONG_URL_MAX - 1);
                continue;
            }
            generate_short_url(p, short_code);
            printf("Short code: %s\n", short_code);
            continue;
        }

        if (strcmp(cmd, "get") == 0) {
            char sc[SHORT_CODE_LEN + 1];
            if (sscanf(buffer + 3, "%7s", sc) != 1) {
                printf("Usage: get <short_code>\n");
                continue;
            }
            char longurl[LONG_URL_MAX];
            if (retrieve_original(sc, longurl, sizeof(longurl))) {
                printf("Original URL: %s\n", longurl);
            } else {
                printf("Not found.\n");
            }
            continue;
        }

        if (strcmp(cmd, "del") == 0) {
            char sc[SHORT_CODE_LEN + 1];
            if (sscanf(buffer + 3, "%7s", sc) != 1) {
                printf("Usage: del <short_code>\n");
                continue;
            }
            if (delete_short(sc)) printf("Deleted mapping %s\n", sc);
            else printf("Not found.\n");
            continue;
        }

        if (strcmp(cmd, "list") == 0) {
            print_all_mappings();
            continue;
        }

        if (strcmp(cmd, "count") == 0)
        {
            count();
        }

        if (strcmp(cmd, "exit") == 0) break;

        printf("Unknown command.\n");
    }

    cleanup_all();
    return 0;
}
