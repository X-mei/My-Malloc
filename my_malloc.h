#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Required Functions */
//First Fit Malloc/Free
void * ts_malloc_lock(size_t size);
void ts_free_lock(void * ptr);

//Best Fit Malloc/Free
void * ts_malloc_nolock(size_t size);
void ts_free_nolock(void * ptr);

//unsigned long get_largest_free_data_segment_size();
//unsigned long get_total_free_size();

/* Basic struct */
//Linked List struct to track the memory
struct _block{
  size_t size;
  struct _block * next;
  struct _block * prev;
}typedef block;


/* Global variables */
//Global constant block header size
#define blk_h_size sizeof(block)
//Normal linked list variable for lock version
block * head_norm = NULL;
block * tail_norm = NULL;
//Linked list using Thread Local Storage for no lock version
__thread block * head_tls = NULL;
__thread block * tail_tls = NULL;
//Thread lock mutex
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
//Funtion prototype for sbrk of two versions
typedef void *(*Fn)(intptr_t);


/* Helper Functions */
//Helper to reduce code redundency
void * myMalloc(size_t size, Fn fp, block ** head, block ** tail);//May need modification
//Free are the same for both version, so call this helper
void myFree(void * ptr, block** head, block** tail);
//Modified sbrk function for no lock version
void * sbrk_lock(intptr_t size);
//Function to find the bf block
block * bfSearch(size_t size, block** head);
//Allocate memory(move break upward)
void * allocateMem(size_t size, Fn fp);//Add lock here for no lock
//Split up current block
block * split(block * old_block, size_t size);
//Insert block at corret position in the free list
void insertBlock(block * block_ptr, block ** head, block ** tail);
//Remove a block from the free list
void removeBlock(block * block_ptr, block ** head, block ** tail);
//Merge block with other free blocks if needed
void mergeBlock(block * block_ptr, block ** head, block ** tail);

