#include "cachelab.h"
#include <stdint.h>

#include <stdio.h>
#include <unistd.h>

#include <limits.h>

#include <stdlib.h>

extern char *optarg;
unsigned long long globalTimestamp = 0;


typedef struct{
    //uint64_t == unsigned long long
    unsigned long long timestamp;
    unsigned long long tags;
    int valid_bit;
}Line;

typedef struct{
    Line** lines;
}Cache;

typedef struct{
    // 2^s (set bits) = S
    int set_bits;
    //cacheLine: E
    int cacheLine;
    //2^b (block bits) = B
    int block_bits;
    //valgrind trace file name
    char* filename;
    //valgrind trace file
    FILE* tracefile;
}Options;

typedef struct{
    //cache hit
    int hit;
    //cache miss
    int miss;
    //cache eviction
    int eviction;
}Result;

typedef struct{
    char operation;
    unsigned long long address;
}Instruction;

Cache InitCache(int S, int E);
void FreeCache(Cache* c, int S, int E);

Result GetInstruction(Cache c, Options opt);
Result ExecInstruction(Instruction i, Cache c, Options opt);

void AccumulateResult(Result* a, Result* b);

unsigned long long GetTimestamp();
void UpdateTimestamp();

//accumulate result
void AccumulateResult(Result* a, Result* b){
    a->hit += b->hit;
    a->miss += b->miss;
    a->eviction += b->eviction;
    return;
}

//update TimeStamp
void UpdateTimestamp(){
    globalTimestamp = globalTimestamp + 1;
    return;
}

//get TimeStamp
unsigned long long GetTimestamp(){
    return globalTimestamp;
}

// execute one instruction
Result ExecInstruction(Instruction i, Cache c, Options opt){
    int isHit = 0;
    int isEvic = 0;
    unsigned long long min_timestamp = LONG_MAX;
    unsigned long long target_line_index = 0;
    Result res = {0,0,0};

    if(i.operation == 'I')return res;

    // printf("Executing %c \n", i.operation);

    if(i.operation == 'L' || i.operation == 'M' || i.operation == 'S'){
        unsigned long long set_index = (i.address >> opt.block_bits) & ~(~0 << opt.set_bits);
        //printf("set index: %llu \n", set_index);

        unsigned long long tag = i.address >> (opt.block_bits + opt.set_bits);
        //printf("tag: %llu \n", tag);

        for(int i=0; i< opt.cacheLine; i++){
            Line l = c.lines[set_index][i];

            if(l.valid_bit == 0 ){
                // empty cache line
                min_timestamp = 0;
                target_line_index = i;
                isEvic = 0;
                continue;
            }

            if(l.tags == tag){
                //cache hit
                //printf("cache hit, target_line_index: %d \n", target_line_index);
                target_line_index = i;
                isHit = 1;
                break;
                
            }else{
                if(min_timestamp > l.timestamp){
                    //cache miss, using LRU
                    min_timestamp = l.timestamp;
                    target_line_index = i;
                    isEvic = 1;
                    continue;
                }
            }
        }

        //update cache
        c.lines[set_index][target_line_index].timestamp = GetTimestamp();
        c.lines[set_index][target_line_index].valid_bit =1;
        c.lines[set_index][target_line_index].tags = tag;
        
        if(isHit == 1){
            //cache hit
            res.hit =1;
        
        }else{
            //can not find the tag or empty lines
            res.miss =1;
        }

        if(isEvic == 1){
            //can not find the tag
            res.eviction = 1;
        } 

        // data modify : data load + data store
        if(i.operation == 'M') res.hit +=1;
        UpdateTimestamp();

    }else{
        printf("exec wrong instruction \n");
    }

    return res;
}

Result GetInstruction(Cache c, Options opt){

    opt.tracefile = fopen(opt.filename, "r");

    /*
      I 0400d7d4,8
        M 0421c7f0,4
        L 04f6b868,8
        S 7ff0005c8,8
      [space]operation address,size
    */
    char operation;
    unsigned long long address;
    int size;
    Result res = {0,0,0};

    if(opt.tracefile != NULL){

        while(fscanf(opt.tracefile," %c %llx,%d",&operation, &address, &size) >0){
            Instruction cur_i;
            cur_i.operation = operation;
            cur_i.address = address;

            // printf("Getting %c, %llx, %d \n",operation, address, size);
            Result cur_i_res = ExecInstruction(cur_i, c, opt);
            //printf("Got %d, %d, %d \n", cur_i_res.hit, cur_i_res.miss, cur_i_res.eviction);

            AccumulateResult(&res, &cur_i_res);
        }
        fclose(opt.tracefile);

    }else{
        printf("failed to open file %s \n",opt.filename);
    }

    return res;
}

void FreeCache(Cache* c, int S, int E){
    for(int i=0; i< S; i++){
        free(c->lines[i]);
    }
    free(c->lines);
    return;
}

Cache InitCache(int S, int E){
    Cache c;
    c.lines = (Line **)malloc(S*sizeof(Line*));

    if(c.lines == NULL){
        printf("no space to allocate S*Line*");
    }

    for(int i=0; i<S; i++){
        c.lines[i] = (Line*)malloc(E*sizeof(Line));
        if(c.lines[i] == NULL){
            printf("no space to allocate E*Line");
        }
        c.lines[i]->timestamp = 0;
        c.lines[i]->tags =0;
        // 1: data in cache, 0: no data in cache
        c.lines[i]->valid_bit =0;
    }
    return c;
}

int main(int argc, char* argv[])
{   
    int ch;
    Options opt;

    Cache cache; 

    while((ch=getopt(argc, argv, "s:E:b:t:")) !=  -1){
        switch (ch)
        {
        case 's':
            opt.set_bits = atol(optarg);
            break;
        
        case 'E':
            opt.cacheLine = atol(optarg);
            break;
        
        case 'b':
            opt.block_bits = atol(optarg);
            break;

        case 't':
            // ./csim -t /home/csapp/Cache_Lab/cachelab-handout/traces/yi.trace
            opt.filename = optarg;
            break;

        default:
            printf("unknown option \n");
            break;
        }
    }
    int cacheS = 1 << opt.set_bits;
    cache = InitCache(cacheS, opt.cacheLine);

    Result res;
    res = GetInstruction(cache, opt);

    FreeCache(&cache, cacheS, opt.cacheLine);

    printSummary(res.hit, res.miss, res.eviction);

    return 0;
}
