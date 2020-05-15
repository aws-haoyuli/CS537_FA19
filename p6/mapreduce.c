// Copyright 2019 Haoyu Li Andrew Harron
#include "mapreduce.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int NUM_FILES;
int NUM_PARTITION;
Mapper mapper;
Reducer reducer;
Partitioner partitioner;
pthread_mutex_t fileLock;
int count = 0;
struct partition *p;
int bits = 0;

struct KV {
    char *key;
    char *value;
    struct KV *next;
};
struct key_entry {
    char *key;
    struct key_entry *next_entry;
    struct KV *head;
    struct KV *tail;
    struct KV *curr_visit;
};
struct partition {
    struct KV *head;
    struct KV *tail;
    struct key_entry *KE_head;
    struct key_entry *curr_visit;
    int KV_num;
    pthread_mutex_t lock;
};

void
init(int argc, char *argv[], Mapper map, Partitioner partition, Reducer reduce,
     int num_partitions) {
    int rc = pthread_mutex_init(&fileLock, NULL);
    assert(rc == 0);
    NUM_FILES = argc - 1;
    NUM_PARTITION = num_partitions;

    partitioner = partition;
    mapper = map;
    reducer = reduce;

    int tmp = num_partitions;
    while (tmp >>= 1) {
        bits++;
    }

    // init partition
    p = (struct partition *) malloc(sizeof(struct partition) * num_partitions);
    for (int i = 0; i < NUM_PARTITION; i++) {
        p[i].head = NULL;
        p[i].tail = NULL;
        p[i].KV_num = 0;
        p[i].KE_head = NULL;
        pthread_mutex_init(&p[i].lock, NULL);
    }
}

char *get_func(char *key, int partition_number) {
    struct partition *part = &p[partition_number];
    struct key_entry *tmp = part->curr_visit;
    while (tmp && strcmp(tmp->key, key) != 0) {
        tmp = tmp->next_entry;
        part->curr_visit = part->curr_visit->next_entry;
    }
    if (tmp && tmp->curr_visit) {
        struct KV *re = tmp->curr_visit;
        tmp->curr_visit = tmp->curr_visit->next;
        return re->value;
    }
    return NULL;
}

void *Map_thread(char *arg[]) {
    // loop all files needed to process
    while (count < NUM_FILES) {
        pthread_mutex_lock(&fileLock);
        char *filename = arg[count++];
        pthread_mutex_unlock(&fileLock);
        (*mapper)(filename);
    }
    return NULL;
}

int compareStr(const void *a, const void *b) {
    char *n1 = (*(struct KV **) a)->key;
    char *n2 = (*(struct KV **) b)->key;
    int rc = strcmp(n1, n2);
    return rc;
}

void *Reduce_thread() {
    while (count < NUM_PARTITION) {
        pthread_mutex_lock(&fileLock);
        int part_num = count;
        struct partition *part = &p[count++];
        pthread_mutex_unlock(&fileLock);
        if (part->KV_num == 0) {
            continue;
        }

        struct KV *kv[part->KV_num];
        struct KV *tmp = part->head;
        for (int i = 0; i < part->KV_num; i++) {
            kv[i] = tmp;
            tmp = tmp->next;
            kv[i]->next = NULL;
        }

        qsort(kv, part->KV_num, sizeof(struct KV *), compareStr);

        struct key_entry *last_kv = malloc(sizeof(struct key_entry));
        part->KE_head = last_kv;
        last_kv->key = kv[0]->key;
        last_kv->next_entry = NULL;
        last_kv->tail = kv[0];
        last_kv->head = kv[0];
        last_kv->curr_visit = kv[0];
        part->curr_visit = last_kv;
        for (int i = 1; i < part->KV_num; i++) {
            if (strcmp(last_kv->key, kv[i]->key)) {
                last_kv->next_entry = malloc(sizeof(struct key_entry));
                last_kv = last_kv->next_entry;
                last_kv->key = kv[i]->key;
                last_kv->next_entry = NULL;
                last_kv->tail = kv[i];
                last_kv->head = kv[i];
                last_kv->curr_visit = kv[i];
            } else {
                last_kv->tail->next = kv[i];
                last_kv->tail = kv[i];
            }
        }

        struct key_entry *ktmp = part->KE_head;
        while (ktmp != NULL) {
            char *key = ktmp->key;
            (*reducer)(key, get_func, part_num);
            ktmp = ktmp->next_entry;
        }

        for (int i = 0; i < part->KV_num; i++) {
            free(kv[i]->key);
            free(kv[i]->value);
            free(kv[i]);
        }

        ktmp = part->KE_head;
        struct key_entry *kktmp;
        while (ktmp != NULL) {
            kktmp = ktmp;
            ktmp = ktmp->next_entry;
            free(kktmp);
        }
    }
    return NULL;
}

void MR_Emit(char *key, char *value) {
    unsigned long partition_number = (*partitioner)(key, NUM_PARTITION);
    // create a new node
    struct KV *new_KV = malloc(sizeof(struct KV));

    new_KV->value = malloc(sizeof(char) * (strlen(value) + 1));
    new_KV->key = malloc(sizeof(char) * (strlen(key) + 1));

    strcpy(new_KV->key, key);
    strcpy(new_KV->value, value);
    new_KV->next = NULL;

    // insert into partition
    pthread_mutex_lock(&p[partition_number].lock);
    if (p[partition_number].head == NULL) {
        p[partition_number].head = new_KV;
    } else {
        p[partition_number].tail->next = new_KV;
    }
    p[partition_number].KV_num++;
    p[partition_number].tail = new_KV;
    pthread_mutex_unlock(&p[partition_number].lock);
}

// return partition number
unsigned long MR_DefaultHashPartition(char *key, int num_partitions) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash * 33 + c;
    return hash % num_partitions;
}

unsigned long MR_SortedPartition(char *key, int num_partitions) {
    unsigned long hash = strtoul(key, NULL, 0) >> (32 - bits);
    return hash;
}


void MR_Run(int argc, char *argv[],
            Mapper map, int num_mappers,
            Reducer reduce, int num_reducers,
            Partitioner partition, int num_partitions) {
    init(argc, argv, map, partition, reduce, num_partitions);

    int kMapThreadsLength = num_mappers;
    pthread_t mapthreads[kMapThreadsLength];
    for (int i = 0; i < num_mappers; i++) {
        pthread_create(&mapthreads[i], NULL, (void *)Map_thread, (argv + 1));
    }

    for (int i = 0; i < num_mappers; i++) {
        pthread_join(mapthreads[i], NULL);
    }

    count = 0;
    int kReduceThreadsLength = num_reducers;
    pthread_t reducethreads[kReduceThreadsLength];
    for (int i = 0; i < num_reducers; i++) {
        pthread_create(&reducethreads[i], NULL, Reduce_thread, NULL);
    }

    for (int i = 0; i < num_reducers; i++) {
        pthread_join(reducethreads[i], NULL);
    }
    free(p);
}
