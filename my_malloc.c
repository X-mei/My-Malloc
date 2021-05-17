#include "my_malloc.h"

/* Malloc and free of version with and without lock */
void * ts_malloc_lock(size_t size){
  assert(pthread_mutex_lock(&lock) == 0);
  void * ptr = myMalloc(size, sbrk, &head_norm, &tail_norm);
  assert(pthread_mutex_unlock(&lock) == 0);
  return ptr;
}

void * ts_malloc_nolock(size_t size){
  void * ptr = myMalloc(size, sbrk_lock, &head_tls, &tail_tls);
  return ptr;
}

void ts_free_lock(void * ptr){
  assert(pthread_mutex_lock(&lock) == 0);
  myFree(ptr, &head_norm, &tail_norm);
  assert(pthread_mutex_unlock(&lock) == 0);
}

void ts_free_nolock(void * ptr){
  myFree(ptr, &head_tls, &tail_tls);
}

//Do malloc based of two different version
void * myMalloc(size_t size, Fn fp, block ** head, block ** tail){
  block * block_ptr = *head;
  //If the list is empty, malloc a new block
  if (*head == NULL){
    assert(*tail == NULL);
    block_ptr = allocateMem(size, fp);
  }
  //If not, try to find a block with adequate
  //size based on the size requirement and mode
  else {
    block_ptr = bfSearch(size, head);
    //malloc a new block if no such block found
    if (!block_ptr){//no such free block
      block_ptr = allocateMem(size, fp);
    }
    else {//found suitable block
      if (block_ptr->size <= size + blk_h_size){
        removeBlock(block_ptr, head, tail);
      }
      else {
        block * block_res = split(block_ptr, size);
        return (block *)((char *)block_res + blk_h_size);
      }
    }
  }
  //Return pointer to data part rather than pointer to the header
  return (block *)((char *)block_ptr + blk_h_size);
}

//Found the minimum block that satisfy the requirement
block * bfSearch(size_t size, block** head){
  block * cur = *head;
  block * res = NULL;
	while (cur != NULL){
		if (cur->size < size){
     cur = cur->next;
      continue;
    }
		else {
			if (res == NULL || cur->size < res->size){
				res = cur;
        //Imediately return when reached minimum to make search faster
				if (res->size == size){
					return res;
				}
			}
		}
		cur = cur->next;
	}
  return res;
}

void * allocateMem(size_t size, Fn fp){
  if (size <= 0) return NULL;
  size_t alloc_size = size + blk_h_size;
  void *ptr = fp((alloc_size));
  if (ptr == (void *)-1) return NULL;
  block * new_block = ptr;
  new_block->size = size;
  new_block->next = NULL;
  new_block->prev = NULL;
  return new_block;
}

void * sbrk_lock(intptr_t size) {
  assert(pthread_mutex_lock(&lock) == 0);
  void * ptr = sbrk(size);
  assert(pthread_mutex_unlock(&lock) == 0);
  return ptr;
}

//Splitting the block by creating a new block of required size
//Old block get a decrease in size, so no change need to apply to the free list
block * split(block * old_block, size_t size){
  block * new_block = (void *)old_block + old_block->size - size;
  new_block->size = size;
  new_block->prev = NULL;
  new_block->next = NULL;
  old_block->size = old_block->size - size - blk_h_size;
  return new_block;
}

//Insert block into the free list
void insertBlock(block * block_ptr, block ** head, block ** tail){
  if (*head == NULL){//free list empty
    assert(*tail == NULL);
    *head = block_ptr;
    *tail = block_ptr;
    assert(block_ptr->next == NULL);
    assert(block_ptr->prev == NULL);
    mergeBlock(block_ptr, head, tail);
    return;
  }
  else if (block_ptr < *head){//comes before head
    (*head)->prev = block_ptr;
    block_ptr->next = *head;
    *head = block_ptr;
    mergeBlock(block_ptr, head, tail);
    return;
  }
  else {//normal case
    block * cur = *head;
    while (cur != NULL){//find the block before inserting block
      if (cur < block_ptr){
        if (cur->next == NULL){//if it is the tail
          cur->next = block_ptr;
          block_ptr->prev = cur;
          block_ptr->next = NULL;
          *tail = block_ptr;
          mergeBlock(block_ptr, head, tail);
          return;
        }
        if (cur->next > block_ptr){//if in between adjacent block
          block_ptr->next = cur->next;
          block_ptr->prev = cur;
          cur->next->prev = block_ptr;
          cur->next = block_ptr;
          mergeBlock(block_ptr, head, tail);
          return;
        }
      }
      else if (cur == block_ptr){return;}//If double freeing
      cur = cur->next;
    }
  }
}

//Merge the just inserted block with its neighbor if needed
void mergeBlock(block * block_ptr, block ** head, block ** tail){
  //Merge with previous block if address if adjacent
  if (block_ptr->prev != NULL){
    if ((void*)block_ptr->prev + block_ptr->prev->size + blk_h_size == (void*)block_ptr){
      block_ptr->prev->size += blk_h_size + block_ptr->size;
      block_ptr->prev->next = block_ptr->next;
      if (block_ptr->next != NULL){block_ptr->next->prev = block_ptr->prev;}
      else {*tail = block_ptr->prev;}
      block_ptr->size = 0;
      block * temp = block_ptr;
      block_ptr = block_ptr->prev;
      temp->prev = NULL;
      temp->next = NULL;
    }
  }
  //Merge with next block if address if adjacent
  if (block_ptr->next != NULL){
    if ((void*)block_ptr + block_ptr->size + blk_h_size == (void*)block_ptr->next){
      block_ptr->size += blk_h_size + block_ptr->next->size;
      block_ptr->next->size = 0;
      block * temp = block_ptr->next;
      block_ptr->next = block_ptr->next->next;
      if (block_ptr->next != NULL){block_ptr->next->prev = block_ptr;}
      else {*tail = block_ptr;}
      temp->prev = NULL;
      temp->next = NULL;
    }
  }
}

//Remove the block passes in
void removeBlock(block * block_ptr, block ** head, block ** tail){
  if (block_ptr->prev == NULL && block_ptr->next == NULL){
    *head = NULL;
    *tail = NULL;
  }
  else if (block_ptr->prev == NULL){
    *head = block_ptr->next;
    (*head)->prev = NULL;
  }
  else if (block_ptr->next == NULL){
    *tail = block_ptr->prev;
    (*tail)->next = NULL;
  }
  else {
    block_ptr->prev->next = block_ptr->next;
    block_ptr->next->prev = block_ptr->prev;
  }
  block_ptr->prev = NULL; block_ptr->next = NULL;
}


void myFree(void * ptr, block ** head, block ** tail){
  if (ptr != NULL) {
    block * block_ptr = (block *)((char *)ptr - blk_h_size);
    insertBlock(block_ptr, head, tail);
    ptr = NULL;
  }
}

