#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define LONG_URL_MAX 1024
#define SHORT_CODE_LEN 7
#define HASH_SIZE 1009

static const char *BASE62 = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

// Scrambling configuration to avoid linear short codes
#define MODULUS (1099511627776ULL)     // 2^40 space (~1.1 trillion unique codes) 
#define PRIME_MULTIPLIER (36779219ULL)

static uint64_t scramble_id(uint64_t sequential_id) {
    if (sequential_id >= MODULUS) {
        return 0;
    }
    uint64_t scrambled_id = (sequential_id * PRIME_MULTIPLIER) % MODULUS;
    return scrambled_id;
}

/* Single node used in both hash tables.
   Each node has two 'next' pointers: one for short-table chaining and one for long-table chaining.
*/
typedef struct Node {
    char short_code[SHORT_CODE_LEN + 1];
    char *long_url;
    struct Node *next_short; 
    struct Node *next_long;  
} Node;

//Two hash-tables pointing to the same nodes (no duplicate payloads).
static Node *short_table[HASH_SIZE];
static Node *long_table[HASH_SIZE];

//global counter for generating unique IDs 
static uint64_t global_id = 1;

// djb2 hashing 
unsigned long hash_str(const char *str) {
    unsigned long hash = 5381;
    unsigned char c;
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

// find node by short code (traverse short_table via next_short) 
Node *find_by_short(const char *short_code) {
    unsigned long h = hash_str(short_code);
    Node *cur = short_table[h];
    while (cur) {
        if (strcmp(cur->short_code, short_code) == 0) return cur;
        cur = cur->next_short;
    }
    return NULL;
}

// find node by long url (traverse long_table via next_long) 
Node *find_by_long(const char *long_url) {
    unsigned long h = hash_str(long_url);
    Node *cur = long_table[h];
    while (cur) {
        if (strcmp(cur->long_url, long_url) == 0) return cur;
        cur = cur->next_long;
    }
    return NULL;
}

// Insert a new node into both tables (node allocated once) 
void insert_mapping(const char *short_code, const char *long_url) {
    Node *node = malloc(sizeof(Node));
    if (!node) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    strcpy(node->short_code, short_code);
    node->long_url = strdup(long_url);
    node->next_short = NULL;
    node->next_long = NULL;

    // insert into short_table (head insertion) 
    unsigned long hs = hash_str(short_code);
    node->next_short = short_table[hs];
    short_table[hs] = node;

    // insert into long_table (head insertion) 
    unsigned long hl = hash_str(long_url);
    node->next_long = long_table[hl];
    long_table[hl] = node;
}

// Unlink node from short_table chain given exact node pointer 
int unlink_from_short_table(Node *node) {
    if (!node) return 0;
    unsigned long hs = hash_str(node->short_code);
    Node *cur = short_table[hs];
    Node *prev = NULL;
    while (cur) {
        if (cur == node) {
            if (prev) prev->next_short = cur->next_short;
            else short_table[hs] = cur->next_short;
            return 1;
        }
        prev = cur;
        cur = cur->next_short;
    }
    return 0;
}

// Unlink node from long_table chain given exact node pointer 
int unlink_from_long_table(Node *node) {
    if (!node) return 0;
    unsigned long hl = hash_str(node->long_url);
    Node *cur = long_table[hl];
    Node *prev = NULL;
    while (cur) {
        if (cur == node) {
            if (prev) prev->next_long = cur->next_long;
            else long_table[hl] = cur->next_long;
            return 1;
        }
        prev = cur;
        cur = cur->next_long;
    }
    return 0;
}

// Remove mapping by short_code: unlink from both tables and free node 
int remove_by_short(const char *short_code) {
    Node *node = find_by_short(short_code);
    if (!node) return 0;

    // unlink from both hash tables 
    unlink_from_short_table(node);
    unlink_from_long_table(node);

    // free payload and node 
    free(node->long_url);
    free(node);
    return 1;
}

// Remove mapping by long_url: unlink from both tables and free node 
int remove_by_long(const char *long_url) {
    Node *node = find_by_long(long_url);
    if (!node) return 0;

    unlink_from_short_table(node);
    unlink_from_long_table(node);

    free(node->long_url);
    free(node);
    return 1;
}

// Generate short URL. If long URL already present, return existing short code. 
void generate_short_url(const char *long_url, char *out_short_code) {
    Node *existing = find_by_long(long_url);
    if (existing) {
        strcpy(out_short_code, existing->short_code);
        return;
    }

    for (;;) {
        char candidate[SHORT_CODE_LEN + 1];
        uint64_t seq = global_id % MODULUS;
        uint64_t scrambled = scramble_id(seq);
        id_to_base62(scrambled, candidate);

        if (!find_by_short(candidate)) {
            insert_mapping(candidate, long_url);
            strcpy(out_short_code, candidate);
            global_id++;
            return;
        }
        global_id++;
    }
}

// Retrieve original long URL given short code. Returns 1 if found. 
int retrieve_original(const char *short_code, char *out_long_url, size_t out_size) {
    Node *n = find_by_short(short_code);
    if (!n) return 0;
    strncpy(out_long_url, n->long_url, out_size - 1);
    out_long_url[out_size - 1] = '\0';
    return 1;
}

// Delete mapping given short code. Returns 1 on success. 
int delete_short(const char *short_code) {
    return remove_by_short(short_code);
}

// Print all mappings by traversing short_table (each node freed/owned once in short_table). 
void print_all_mappings() {
    printf("Current mappings (short -> long):\n");
    for (int i = 0; i < HASH_SIZE; ++i) {
        Node *cur = short_table[i];
        while (cur) {
            printf("%s -> %s\n", cur->short_code, cur->long_url);
            cur = cur->next_short;
        }
    }
}

/* Clean-up: iterate short_table and free all nodes once.
   After freeing through short_table, clear long_table buckets.
*/
void cleanup_all() {
    for (int i = 0; i < HASH_SIZE; ++i) {
        Node *s = short_table[i];
        while (s) {
            Node *t = s->next_short;
            free(s->long_url);
            free(s);
            s = t;
        }
        short_table[i] = NULL;
    }
    //long_table still holds dangling pointers now; clear them to NULL 
    for (int i = 0; i < HASH_SIZE; ++i) {
        long_table[i] = NULL;
    }
    printf("Clean-Up Done!!\nExiting Code...\n");
}

// Count non-empty buckets in both tables (keeps previous behavior) 
void count() {
    int short_count = 0, long_count = 0;
    for (int i = 0; i < HASH_SIZE; i++) {
        if (short_table[i]) short_count++;
        if (long_table[i]) long_count++;
    }
    printf("Short_table count->%d\nLong_table count->%d\n", short_count, long_count);
}

int main() {
    char cmd[16];
    char buffer[LONG_URL_MAX];
    char short_code[SHORT_CODE_LEN + 1];

    printf("URL Shortener CLI\n");
    printf("Commands: gen <long_url>, get <short_code>, del <short_code>, list, count, exit\n");

    while (1) {
        printf("> ");
        if (!fgets(buffer, sizeof(buffer), stdin)) break;
        buffer[strcspn(buffer, "\n")] = 0;
        if (strlen(buffer) == 0) continue;

        if (sscanf(buffer, "%15s", cmd) != 1) continue;

        if (strcmp(cmd, "gen") == 0) {
            char *p = buffer + 3;
            while (*p == ' ') p++;
            if (*p == '\0') {
                printf("Usage: gen <long_url>\n");
                continue;
            }
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

        if (strcmp(cmd, "count") == 0) {
            count();
            continue;
        }

        if (strcmp(cmd, "exit") == 0) break;

        printf("Unknown command.\n");
    }

    cleanup_all();
    return 0;
}
