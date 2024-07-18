#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    int *valid;
    int *tag;
    int *fifoCounter;
    int *lruCounter;
    int *dirty;
} CacheSet;

void initializeCache(CacheSet *cache, int numSets, int associativity) {
    for (int i = 0; i < numSets; ++i) {
        cache[i].valid = (int *)malloc(associativity * sizeof(int));
        cache[i].tag = (int *)malloc(associativity * sizeof(int));
        cache[i].fifoCounter = (int *)malloc(associativity * sizeof(int));
        cache[i].lruCounter = (int *)malloc(associativity * sizeof(int));
        cache[i].dirty = (int *)malloc(associativity * sizeof(int));
        for (int j = 0; j < associativity; ++j) {
            cache[i].valid[j] = 0;
            cache[i].tag[j] = -1;
            cache[i].fifoCounter[j] = 0;
            cache[i].lruCounter[j] = 0;
            cache[i].dirty[j] = 0;
        }
    }
}

int totalHits = 0;
int totalMisses = 0;

void simulateCacheAccess(char mode, int address, CacheSet *cache, int cacheSize, int blockSize, int associativity,
                          const char *replacementPolicy, const char *writebackPolicy, int lines) {

    int numSets;
    if (associativity == 0) {
        numSets = 1;
    } else {
        numSets = cacheSize / (blockSize * associativity);
    }

    int blockOffsetBits = log2(blockSize);
    int indexBits = log2(numSets);
    int tagBits = 32 - blockOffsetBits - indexBits;

    int index = (address >> blockOffsetBits) & ((1 << indexBits) - 1);
    int tag = (address >> (blockOffsetBits + indexBits)) & ((1 << tagBits) - 1);


    int hit = 0;
    int hitIndex = -1;

    if (associativity == 0) {
        for (int i = 0; i < lines; ++i) {
            if (cache[index].valid[i] && cache[index].tag[i] == tag) {
                hit = 1;
                hitIndex = i;

                if (strcmp(replacementPolicy, "LRU") == 0) {
                    for (int j = 0; j < lines; ++j) {
                        cache[index].lruCounter[j]++;
                    }
                    cache[index].lruCounter[i] = 0; 
                }

                break;
            }
        }
    } else {
        for (int i = 0; i < associativity; ++i) {
            if (cache[index].valid[i] && cache[index].tag[i] == tag) {
                hit = 1;
                hitIndex = i;

                if (strcmp(replacementPolicy, "LRU") == 0) {
                    for (int j = 0; j < associativity; ++j) {
                        cache[index].lruCounter[j]++;
                    }
                    cache[index].lruCounter[i] = 0; // Reset the LRU counter for the hit line
                }

                break;
            }
        }
    }

    int replaceIndex;
    int numlines;
    int setIndex;
    if (!hit) {
        numlines = (associativity == 0) ? lines : associativity;
        setIndex = (associativity == 0) ? index : index;

        if (strcmp(replacementPolicy, "FIFO") == 0) {
            replaceIndex = 0;
            for (int i = 1; i < numlines; ++i) {
                if (cache[setIndex].fifoCounter[i] < cache[setIndex].fifoCounter[replaceIndex]) {
                    replaceIndex = i;
                }
            }
            int maxFifoCounter = 0;
            for (int i = 0; i < numlines; ++i) {
                if (cache[setIndex].fifoCounter[i] > maxFifoCounter) {
                    maxFifoCounter = cache[setIndex].fifoCounter[i];
                }
            }
            cache[setIndex].fifoCounter[replaceIndex] = maxFifoCounter + 1;
        } else if (strcmp(replacementPolicy, "LRU") == 0) {
            replaceIndex = 0;
            for (int i = 1; i < numlines; ++i) {
                if (cache[setIndex].lruCounter[i] < cache[setIndex].lruCounter[replaceIndex]) {
                    replaceIndex = i;
                }
            }
            int maxLruCounter = 0;
            for (int i = 0; i < numlines; ++i) {
                if (cache[setIndex].lruCounter[i] > maxLruCounter) {
                    maxLruCounter = cache[setIndex].lruCounter[i];
                }
            }
            cache[setIndex].lruCounter[replaceIndex] = maxLruCounter + 1;
        } else if (strcmp(replacementPolicy, "RANDOM") == 0) {
            replaceIndex = rand() % numlines;
        } else {
            printf("Unsupported replacement policy\n");
            return;
        }

        if (mode == 'R' || (mode == 'W' && strcmp(writebackPolicy, "WB") == 0)) {
            cache[setIndex].valid[replaceIndex] = 1;
            cache[setIndex].tag[replaceIndex] = tag;
        }

        if (mode == 'W') {
            if (strcmp(writebackPolicy, "WT") == 0) {
                cache[setIndex].dirty[replaceIndex] = 0;
            } else if (strcmp(writebackPolicy, "WB") == 0) {
                cache[setIndex].dirty[replaceIndex] = 1;
            }
        }
    }

    if (hit) {
        totalHits++;
        printf("Address: 0x%08x, Set: 0x%x, HIT, Tag: 0x%x\n", address, setIndex, tag);
    } else {
        printf("Address: 0x%08x, Set: 0x%x, MISS, Tag: 0x%x\n", address, setIndex, tag);
        totalMisses++;
    }
}

int countLines(const char *filename) {
    FILE *file = fopen(filename, "r");

    int lineCount = 0;
    char buffer[10000];

    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        lineCount++;
    }

    fclose(file);
    return lineCount;
}

int main() {
    FILE *configFile = fopen("cache.config", "r");
    if (!configFile) {
        printf("Error opening cache.config\n");
        return 1;
    }

    int cacheSize, blockSize, associativity;
    char replacementPolicy[10], writebackPolicy[3];

    fscanf(configFile, "%d %d %d %s %s", &cacheSize, &blockSize, &associativity, replacementPolicy, writebackPolicy);
    fclose(configFile);

    if (cacheSize <= 0 || blockSize <= 0 || associativity < 0 || associativity > 16) {
        printf("Invalid cache parameters\n");
        return 1;
    }

    if (strcmp(replacementPolicy, "FIFO") != 0 && strcmp(replacementPolicy, "LRU") != 0 &&
        strcmp(replacementPolicy, "RANDOM") != 0) {
        printf("Invalid replacement policy\n");
        return 1;
    }

    if (strcmp(writebackPolicy, "WB") != 0 && strcmp(writebackPolicy, "WT") != 0) {
        printf("Invalid writeback policy\n");
        return 1;
    }

    int numSets;
    if (associativity == 0) {
        numSets = 1;
    } else {
        numSets = cacheSize / (blockSize * associativity);
    }

    CacheSet *cache = (CacheSet *)malloc(numSets * sizeof(CacheSet));
    initializeCache(cache, numSets, associativity);

    FILE *accessFile = fopen("cache.access", "r");


    char mode;
    int address;
    const char *filename = "cache.access";
    int lines = countLines(filename);

    while (fscanf(accessFile, " %c: %x", &mode, &address) == 2) {
        simulateCacheAccess(mode, address, cache, cacheSize, blockSize, associativity, replacementPolicy, writebackPolicy, lines);
    }
    printf("Total Hits: %d\n", totalHits);
    printf("Total Misses: %d\n", totalMisses);

    fclose(accessFile);
    free(cache);

    return 0;
}
