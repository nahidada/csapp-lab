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
    "nahida@cs.cmu.edu",
    "",
    ""
};
#define DEBUG_LEVEL 1

#define DEBUG_LOG_FILE "debug.log"

#define DEBUG_PRINT(fmt, ...) \
    do { \
        FILE *debug_log_file = fopen(DEBUG_LOG_FILE, "a"); \
        if (debug_log_file != NULL) { \
            fprintf(debug_log_file, "[%s:%d:%s] " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
            fclose(debug_log_file); \
        } \
    } while (0)


#define LIST_PRINT(fmt, ...) \
    do { \
        FILE *debug_log_file = fopen(DEBUG_LOG_FILE, "a"); \
        if (debug_log_file != NULL) { \
            fprintf(debug_log_file, "[%s:%d:%s] " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
            fclose(debug_log_file); \
        } \
    } while (0)

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
#define GET_NEXTBP(bp) ((char *)bp + GET_SIZE(HEADER(bp)))
// #define GET_PREVBP(bp) ((char *)bp - GET_SIZE(((char *)(bp) - DSIZE)))
#define GET_PREVBP(bp) ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))

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
static void print_seglist();

//extend heap
static void * extend_heap(size_t words);

#define SEG_LEN 9

static void print_seglist(){
    int i=0;
    char * seg_root;
    char * node;

    for(i=0; i<SEG_LEN; i++){
        seg_root = seglist + i * WSIZE;
        node = seglist + i * WSIZE;
        
        LIST_PRINT("%d th root at %p", i, seg_root);

        while (SUCC(node))
        {
            node = SUCC(node);
            LIST_PRINT("--> %p, %d, %d",node , GET_SIZE(HEADER(node)),GET_SIZE(FOOTER(node)) );   
        }

        LIST_PRINT("--> %p \n",SUCC(node)); 

        while (node != seg_root)
        {   
            LIST_PRINT("<-- %p, %d, %d", node, GET_SIZE(HEADER(node)), GET_SIZE(FOOTER(node)));
            node = PRED(node);
             
        }

        LIST_PRINT("<-- %p \n",node);
    }
    return;
}
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

    DEBUG_PRINT("seglist %p", seglist);

    int i =0;
    for(i=0; i<SEG_LEN; i++){
        PUT(heap_listp+(i*WSIZE), NULL);
        // DEBUG_PRINT("seglist %d th root %p",i, heap_listp+(i*WSIZE));
    }
    heap_listp = heap_listp + ((SEG_LEN-1) * WSIZE);

    //prologue header
    PUT(heap_listp+1*WSIZE, PACK(DSIZE,1));
    DEBUG_PRINT("prologue header %p", heap_listp+1*WSIZE);
    
    //prologue footer
    PUT(heap_listp+2*WSIZE, PACK(DSIZE,1));
    DEBUG_PRINT("prologue footer %p", heap_listp+2*WSIZE);

    //epilogue header
    PUT(heap_listp+3*WSIZE, PACK(0,1));
    DEBUG_PRINT("epilogue header %p", heap_listp+3*WSIZE);

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

    //DEBUG_PRINT("bp %p, header_bp %p, footer_bp %p, size %d, alloc %d", bp, HEADER(bp), FOOTER(bp), size, 0);

    //new epilogue block, update the address of epilogue block
    PUT(HEADER(GET_NEXTBP(bp)), PACK(0,1));    
    //DEBUG_PRINT("new epilogue blk %p, alloc %d", HEADER(GET_NEXTBP(bp)), 1);

    
    PUT(GET_PREDP(bp), NULL);
    PUT(GET_SUCCP(bp), NULL);

    bp = imme_coalesce(bp);
    bp = add_blk(bp);

    return bp;
}

static void * imme_coalesce(void* bp){
    
    //the bp of next blk
    char * next_bp = GET_NEXTBP(bp);
    //the bp of prev blk
    char * prev_bp = GET_PREVBP(bp);

    int next_bp_alloc = GET_ALLOC(HEADER(next_bp));
    int prev_bp_alloc = GET_ALLOC(FOOTER(prev_bp));

    size_t next_bp_size = GET_SIZE(HEADER(next_bp));
    size_t prev_bp_size = GET_SIZE(FOOTER(prev_bp));


    // DEBUG_PRINT("bp %p, next_bp %p, next_bp_alloc %d, next_bp_size %d",
    //  bp, next_bp, next_bp_alloc, next_bp_size);

    // DEBUG_PRINT("bp %p, prev_bp %p, prev_bp_alloc %d, prev_bp_size(footer) %d, prev_bp_size(header) %d ",
    //  bp, prev_bp, prev_bp_alloc, prev_bp_size, GET_SIZE(HEADER(prev_bp)));

    // DEBUG_PRINT("bp %p, prev_bp %p,  prev_bp_header %p, prev_bp_footer %p",bp, prev_bp ,HEADER(prev_bp), FOOTER(prev_bp));
    
    size_t bp_size = GET_SIZE(HEADER(bp));

    if(next_bp_alloc && prev_bp_alloc){
        
        return bp;

    }else if( next_bp_alloc && ! prev_bp_alloc ){

        //prev_blk is free
        if(prev_bp == bp){

            char * tmp_prev_footer = ((char *)bp - DSIZE);
            size_t tmp_prev_s = GET_SIZE(tmp_prev_footer);
            char * tmp_prev = bp - tmp_prev_s;
            // DEBUG_PRINT("tmp %p, tmp_prev %p, tmp_prev_s %d, tmp_prev_footer %p", bp, tmp_prev, tmp_prev_s, tmp_prev_footer );

            PUT(HEADER(bp), PACK(GET_SIZE(HEADER(bp)), 0));
            PUT(FOOTER(bp), PACK(GET_SIZE(HEADER(bp)), 0));
        }else{
            bp_size += GET_SIZE(HEADER(prev_bp));
            // DEBUG_PRINT("prev_bp %p, new_size %d",prev_bp, bp_size);

            delete_blk(prev_bp);
            char * prev_bp_header = HEADER(prev_bp);
            char * prev_bp_footer = FOOTER(prev_bp);

            char *bp_header = HEADER(bp);
            char *bp_footer = FOOTER(bp);

            PUT(prev_bp_header, PACK(bp_size, 0));
            // DEBUG_PRINT("prev_bp %p, header_prev_bp %p, size %d, alloc 0", prev_bp, prev_bp_header, bp_size);
            
            PUT(prev_bp_footer, PACK(0, 1));
            // DEBUG_PRINT("prev_bp %p, footer_prev_bp %p, size 0, alloc 0", prev_bp, prev_bp_footer);

            PUT(bp_header, PACK(0, 1));
            // DEBUG_PRINT("bp %p, header_bp %p, size 0, alloc 0", bp, bp_header);

            PUT(bp_footer, PACK(bp_size, 0));
            // DEBUG_PRINT("bp %p, footer_bp %p, size %d, alloc 0", bp, bp_footer, bp_size);

        }
        return prev_bp;
    
    }else if( ! next_bp_alloc && prev_bp_alloc ){
        bp_size += GET_SIZE(HEADER(next_bp));
        
        // next_blk is free
        delete_blk(next_bp);

        char * bp_header = HEADER(bp);
        char * bp_footer = FOOTER(bp);

        char * next_bp_header = HEADER(next_bp);
        char * next_bp_footer = FOOTER(next_bp);

        PUT(bp_header, PACK(bp_size, 0));
        PUT(bp_footer, PACK(0, 1));

        PUT(next_bp_header, PACK(0, 1));
        PUT(next_bp_footer, PACK(bp_size, 0));

        // DEBUG_PRINT("bp header %p, next_bp_footer %p, size %d",bp_header, next_bp_footer, bp_size);
        return bp;
        
    }else{
        // prev and next blk are free.
        delete_blk(prev_bp);
        delete_blk(next_bp);

        bp_size = bp_size + GET_SIZE(HEADER(prev_bp));
        bp_size = bp_size + GET_SIZE(HEADER(next_bp));

        char * prev_bp_header = HEADER(prev_bp);
        char * prev_bp_footer = FOOTER(prev_bp);

        char* bp_header = HEADER(bp);
        char* bp_footer = FOOTER(bp);

        char *next_bp_header = HEADER(next_bp);
        char *next_bp_footer = FOOTER(next_bp);

        PUT(prev_bp_header, PACK(bp_size, 0));
        PUT(prev_bp_footer, PACK(0, 1));

        PUT(bp_header, PACK(0, 1));
        PUT(bp_footer, PACK(0, 1));

        PUT(next_bp_header,PACK(0, 1));
        PUT(next_bp_footer, PACK(bp_size, 0));

        // DEBUG_PRINT("prev_bp header %p, next_bp_footer %p, size %d",prev_bp_header, next_bp_footer, bp_size);

        return prev_bp;
    }
}

static void delete_blk(char * bp){
    
    //update succ & pred
    char * pred_bp = PRED(bp);
    char * succ_bp = SUCC(bp);

    // DEBUG_PRINT("bp %p, pred_bp %p, succ_bp %p", bp, pred_bp, succ_bp);

    PUT(GET_SUCCP(pred_bp), succ_bp);
    if(succ_bp != NULL){
        PUT(GET_PREDP(succ_bp), pred_bp);
    }

    return;
}

static char * add_blk(char * bp){

    size_t size = GET_SIZE(HEADER(bp));
    int idx = get_index(size);
    char * seglist_root = seglist + idx * WSIZE;

    // DEBUG_PRINT("bp %p, header_bp %p, footer_bp %p, size %d, alloc %d", bp, HEADER(bp), FOOTER(bp), 
    //  GET_SIZE(HEADER(bp)), GET_ALLOC(HEADER(bp)));

    //call insertion policy
    bp = LIFO(bp, seglist_root);
    // DEBUG_PRINT("bp %p, size_header %d, size_footer %d", bp, GET_SIZE(HEADER(bp)), GET_SIZE(FOOTER(bp)));
    
    return bp;
}

static char* LIFO(char *bp, char *root){
    
    //update pred & succ
    if(SUCC(root) == NULL){

        // DEBUG_PRINT("root %p, succ_root %p, bp %p", root, SUCC(root), bp);
        PUT(GET_SUCCP(bp), NULL);
    
    }else{
        char* old_succ = SUCC(root);
        // DEBUG_PRINT("root %p, bp %p, old_succ %p", root, SUCC(root), bp, old_succ);
        PUT(GET_SUCCP(bp), old_succ);
        PUT(GET_PREDP(old_succ), bp);
    }

    PUT(GET_SUCCP(root), bp);
    PUT(GET_PREDP(bp), root);

    // DEBUG_PRINT("bp %p, size_header %d, size_footer %d", bp, GET_SIZE(HEADER(bp)), GET_SIZE(FOOTER(bp)));
    
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
        // DEBUG_PRINT("root %p, succ %p", seglist_root, succ);

        while(succ != NULL ){
            if(GET_SIZE(HEADER(succ)) >= size){
                // DEBUG_PRINT("succ %p, succ_header %p, succ_footer %p", succ, HEADER(succ), FOOTER(succ));
                // DEBUG_PRINT("succ %p , size %d, size %d", succ, GET_SIZE(HEADER(succ)), GET_SIZE(FOOTER(succ)));
                return succ;
            }else{
                // DEBUG_PRINT("succ %p, succ_size %d", succ, GET_SIZE(HEADER(succ)));
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
    // DEBUG_PRINT("bp %p, bp_header %p, bp_footer %p, bp_size_header %d, bp_size_footer %d",
    // bp, HEADER(bp), FOOTER(bp),GET_SIZE(HEADER(bp)), GET_SIZE(FOOTER(bp)));

    char * remain_blk = GET_NEXTBP(bp); 

    // DEBUG_PRINT("bp %p, size %d, remain_blk %p, remain size %d", bp, size, remain_blk, remain_size);
    // DEBUG_PRINT("remain_blk %p, remain_blk_header %p", 
    // remain_blk, HEADER(remain_blk));

    PUT(HEADER(remain_blk), PACK(remain_size, 0));

    // DEBUG_PRINT("remain_blk %p, remain_blk_header %p, remain_blk_footer %p", 
    // remain_blk, HEADER(remain_blk), FOOTER(remain_blk));
    
    PUT(FOOTER(remain_blk), PACK(remain_size, 0));
    
    add_blk(remain_blk);

    return;
}
/*
size : mm_malloc need bytes
bp : free blk that contain at least size bytes
*/
static void place(char * bp, size_t size){
    size_t remain_size;
    remain_size = GET_SIZE(HEADER(bp)) - size;
    // DEBUG_PRINT("bp %p, size %d", bp, size);
    delete_blk(bp);

    if(remain_size >= 2*DSIZE){
        //splitting policy
        // DEBUG_PRINT("bp %p, size %d, remain_size %d", bp, size, remain_size);
        split_blk(bp, size, remain_size);
        // DEBUG_PRINT("bp %p, size %d, remain_size %d", bp, size, remain_size);

    }else{   
        //set allocated flag
        // DEBUG_PRINT("bp %p, bp_header %p, bp_footer %p",bp, HEADER(bp), FOOTER(bp));
        PUT(HEADER(bp), PACK(GET_SIZE(HEADER(bp)), 1));
        PUT(FOOTER(bp), PACK(GET_SIZE(HEADER(bp)), 1));
        // DEBUG_PRINT("bp %p, bp_header %p, bp_footer %p",bp, HEADER(bp), FOOTER(bp));
    }
    return;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{   

    // DEBUG_PRINT("size %d", size);
    size_t asize;
    char *bp;

    asize = ( size <= DSIZE ? 2*DSIZE : DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE) );

    bp = first_fit(asize);

    // DEBUG_PRINT("first_fit return, bp %p, asize %d", bp, asize);
   

    if(bp !=NULL){

        // DEBUG_PRINT("bp %p, asize(mm_malloc need size) %d, bp_size_header %d, bp_size_footer %d", 
        // bp, asize, GET_SIZE(HEADER(bp)), GET_SIZE(FOOTER(bp)));

        place(bp, asize);

        // DEBUG_PRINT("place return, bp %p, asize %d, bp_size_header %d, bp_size_footer %d", 
        // bp, asize, GET_SIZE(HEADER(bp)), GET_SIZE(FOOTER(bp)));

        return bp;
    
    }else{

        // DEBUG_PRINT("bp %p, asize %d", bp, asize);

        bp = extend_heap(MAX(CHUNKSIZE, (asize+WSIZE-1))/WSIZE);
        if(bp == NULL){
            return NULL;
        }

        place(bp, asize);

        // DEBUG_PRINT("extend_heap, return bp %p, size %d",bp, GET_SIZE(HEADER(bp)));
        return bp;
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr){

    size_t size = GET_SIZE(HEADER(ptr));
    // DEBUG_PRINT("ptr %p, size %d", ptr, size);

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
    // DEBUG_PRINT("bp %p, size %d", ptr, size);

    if(ptr == NULL){
        return mm_malloc(size);
    }

    if(size == 0){
        mm_free(ptr);
        return NULL;
    }

    size_t target_size;
    target_size = ( size <= DSIZE ? 2*DSIZE : DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE) );

    void *oldbp = ptr;
    size_t oldbp_size = GET_SIZE(HEADER(oldbp));

    void *newbp;
    newbp = imme_coalesce(oldbp);
    size_t new_size = GET_SIZE(HEADER(newbp));

    PUT(HEADER(newbp), PACK(new_size, 1));
    PUT(FOOTER(newbp), PACK(new_size, 1));
    // DEBUG_PRINT("set the allocated flag of bp %p to 1", newbp);

    if(newbp != ptr){
        memcpy(newbp, ptr, oldbp_size - DSIZE);
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














