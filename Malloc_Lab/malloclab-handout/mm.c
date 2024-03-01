/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Nahida_team",
    /* First member's full name */
    "Nahida",
    /* First member's email address */
    "nahida@cs.cmu.edu"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 4

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// WORD SIZE IS 4 BYTES
#define WSIZE 4
// Doudle word size
#define DSIZE 8
#define CHUNKSIZE (1<< 12)

#define MAX(x,y) ((x)>(y) ? (x): (y))

//allocated:1, free:0
#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int*) (p))
#define PUT(p, val) (*(unsigned int* )(p) = (val))

//size = header + payload + footer
#define GET_SIZE(p) (GET(p) & ~0x7)
//allocated : 1, free : 0
#define GET_ALLOC(p) (GET(p) & 0x1)

//get header and footer ptr
#define HEADER(bp) ((char *)(bp) - WSIZE) 
#define FOOTER(bp) ((char *)(bp) + GET_SIZE(HEADER(bp)) - DSIZE)

//get next and prev block ptr
#define GET_NEXTBP(bp) (bp + GET_SIZE(((char*)(bp) - WSIZE)))
#define GET_PREVBP(bp) (bp - GET_SIZE(((char *)(bp) - DSIZE)))

//get pred and succ block ptr at the free blk
#define GET_PREDP(bp) ((char *)(bp) + WSIZE)
#define GET_SUCCP(bp) ((char *)(bp))

//get the val of pred and succ block ptr at the free blk
#define PRED(bp) (GET(GET_PREDP(bp)))
#define SUCC(bp) (GET(GET_SUCCP(bp))) 

static char * heap_listp;
static char * seglist;

//coalesce policy
static void * imme_coalesce(void * bp);

//splitting policy
static void split_blk(char *bp, size_t size, size_t remain_size);

//placement policy
static char* first_fit(size_t size);
static void place(char * bp, size_t size);

//insertion policy
static char* LIFO(char *bp, char *root);

//seglist opertation
static void delete_blk(char * bp);
static char* add_blk(char * bp);
static int get_index(size_t size);


//extend heap
static void * extend_heap(size_t words);


#define SEG_LEN 9
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    char * heap_listp = mem_sbrk((SEG_LEN + 3) * WSIZE);
    if(heap_listp == (void *) -1 ){
        return -1;
    }
    seglist = heap_listp;
    int i =0;
    for(i=0; i<SEG_LEN; i++){
        PUT(heap_listp+(i*WSIZE), NULL);
    }
    heap_listp = heap_listp + (SEG_LEN * WSIZE);
    //prologue header
    PUT(heap_listp+(SEG_LEN+1), PACK(WSIZE,1));
    //prologue footer
    PUT(heap_listp+(SEG_LEN+2), PACK(WSIZE,1));
    //epilogue header
    PUT(heap_listp+(SEG_LEN+3), PACK(0,1));

    if(extend_heap(CHUNKSIZE/WSIZE) == NULL){
        return -1;
    }
    return 0;
}

static void * extend_heap(size_t words){
    void * bp;
    size_t size;

    if(words < 4){
        //min free block is 4 word.
        return NULL;
    }

    size = (words %2)==0 ? words*WSIZE : (words+1)*WSIZE;

    bp = mem_sbrk(size);
    if(bp == (void *)-1){
        return NULL;
    }

    PUT(HEADER(bp), PACK(size, 0));
    PUT(FOOTER(bp), PACK(size, 0));

    //new epilogue block, update the address of epilogue block
    PUT(HEADER(GET_NEXTBP(bp)), PACK(0,1));

    PUT(GET_PREDP(bp), NULL);
    PUT(GET_SUCCP(bp), NULL);

    bp = imme_coalesce(bp);
    bp = add_blk(bp);

    return bp;
}

static void * imme_coalesce(void * bp){
    char * next_blk = GET_NEXTBP(bp);
    char * prev_blk = GET_PREVBP(bp);

    int next_blk_alloc = GET_ALLOC(next_blk);
    int prev_blk_alloc = GET_ALLOC(prev_blk);

    size_t size = GET_SIZE(HEADER(bp));

    if(next_blk_alloc && prev_blk_alloc){
        
        return bp;

    }else if( next_blk_alloc && ! prev_blk_alloc ){

        size += GET_SIZE(HEADER(prev_blk));

        //prev_blk is free
        delete_blk(prev_blk);

        PUT(HEADER(prev_blk), PACK(size, 0));
        PUT(FOOTER(prev_blk), PACK(0, 0));
        PUT(HEADER(bp), PACK(0, 0));
        PUT(FOOTER(bp), PACK(size, 0));
        return prev_blk;
    
    }else if( ! next_blk_alloc && prev_blk_alloc ){
        size += GET_SIZE(HEADER(next_blk));
        
        // next_blk is free
        delete_blk(next_blk);

        PUT(HEADER(bp), PACK(size, 0));
        PUT(FOOTER(bp), PACK(0, 0));

        PUT(HEADER(next_blk), PACK(0, 0));
        PUT(FOOTER(next_blk), PACK(size, 0));
        return bp;
        
    }else{
        // prev and next blk are free.
        delete_blk(prev_blk);
        delete_blk(next_blk);

        size += GET_SIZE(HEADER(prev_blk));
        size += GET_SIZE(HEADER(next_blk));

        PUT(HEADER(prev_blk), PACK(size, 0));
        PUT(FOOTER(prev_blk), PACK(0, 0));

        PUT(HEADER(bp), PACK(0, 0));
        PUT(FOOTER(bp), PACK(0, 0));

        PUT(HEADER(next_blk), PACK(0, 0));
        PUT(FOOTER(next_blk), PACK(size, 0));

        return prev_blk;
    }
}

static void delete_blk(char * bp){

    char * pred_blk = PRED(bp);
    char * succ_blk = SUCC(bp);

    PUT(GET_SUCCP(pred_blk), succ_blk);
    if(succ_blk != NULL){
        PUT(GET_PREDP(succ_blk), pred_blk);
    }

    return;
}

static char * add_blk(char * bp){
    size_t size = GET_SIZE(HEADER(bp));
    int idx = get_index(size);
    char * seglist_root = seglist + idx * WSIZE;

    //call insertion policy
    bp = LIFO(bp, seglist_root);

    return bp;
}

static char* LIFO(char *bp, char *root){
    if(SUCC(root) == NULL){

        PUT(GET_SUCCP(bp), NULL);
    
    }else{
        char* old_succ = SUCC(root);
        PUT(GET_SUCCP(bp), old_succ);
        PUT(GET_PREDP(old_succ), bp);
    }

    PUT(GET_SUCCP(root), bp);
    PUT(GET_PREDP(bp), root);

    return bp;
}

static void AddressOreder(char *bp, char * root){
    return;
}

static int get_index(size_t size){
    int idx = 0;
    
    if(size > 4096){
        return 8;
    }

    size = size >> 5;
    while(size){
       size = size >> 1;
       idx++;
    }

    return idx;
}

static char* first_fit(size_t size){
    int idx = get_index(size);
    char * seglist_root;
    char * succ;

    while(idx <=8){
        seglist_root = seglist + idx * WSIZE; 
        succ = SUCC(seglist_root);

        while(succ != NULL ){
            if(GET_SIZE(HEADER(succ)) >= size){
                return succ;
            }else{
                succ = SUCC(succ);
            }
        }
        idx++;
    }

    return NULL;
}

static void split_blk(char *bp, size_t size, size_t remain_size){
    PUT(HEADER(bp), PACK(size, 1));
    PUT(FOOTER(bp), PACK(size, 1));

    char * remain_blk = GET_NEXTBP(bp); 
    PUT(HEADER(remain_blk), PACK(remain_size, 0));
    PUT(FOOTER(remain_blk), PACK(remain_size, 0));
    add_blk(remain_blk);

    return;
}

static void place(char * bp, size_t size){
    size_t remain_size;
    remain_size = GET_SIZE(HEADER(bp)) - size;
    delete_blk(bp);

    if(remain_size >= 2*DSIZE){
        //splitting policy
        split_blk(bp, size, remain_size);

    }else{   
        //set allocated flag
        PUT(HEADER(bp), PACK(GET_SIZE(HEADER(bp)), 1));
        PUT(FOOTER(bp), PACK(GET_SIZE(HEADER(bp)), 1));

    }
    return;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    char *bp;

    asize = (size < 2*DSIZE ? 2*DSIZE : (size + 2*DSIZE +DSIZE) & ~(0xF));

    bp = first_fit(asize);

    if(bp !=NULL){
        place(bp, asize);
        return bp;
    }else{
        bp = extend_heap(MAX(CHUNKSIZE, (asize+WSIZE-1))/WSIZE);
        if(bp == NULL){
            return NULL;
        }

        place(bp, asize);
        return bp;
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr){
    size_t size = GET_SIZE(HEADER(ptr));

    PUT(HEADER(ptr), PACK(size, 0));
    PUT(FOOTER(ptr), PACK(size, 0));
    
    ptr = imme_coalesce(ptr);
    add_blk(ptr);
    
    return;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size){

    if(ptr == NULL){
        return mm_malloc(size);
    }

    if(size == 0){
        mm_free(ptr);
        return NULL;
    }

    size_t target_size;
    target_size = (size < 2*DSIZE ? 2*DSIZE : ((size + DSIZE + (DSIZE -1))) && ~(0xF));

    void *oldbp = ptr;
    void *newbp;

    newbp = imme_coalesce(oldbp);
    size_t new_size = GET_SIZE(HEADER(newbp));

    PUT(HEADER(newbp), PACK(new_size, 1));
    PUT(FOOTER(newbp), PACK(new_size, 1));

    if(newbp != ptr){
        memcpy(newbp, ptr, GET_SIZE(HEADER(ptr)) - DSIZE);
    }

    if(new_size == target_size){
        return newbp;
    }else if(new_size > target_size){
        //check the coalesced free block
        size_t remain_size = new_size - target_size;
        if(remain_size > 2*DSIZE){
            //split the coalesced free block
            split_blk(newbp, target_size, remain_size);
        }
        return newbp;
    
    }else{
        //apply a new free block
        ptr = mm_malloc(target_size);
        if(ptr == NULL){
            return NULL;
        }
        
        memcpy(ptr, newbp, (new_size - DSIZE));
        mm_free(newbp);
        return ptr;

    }

}














