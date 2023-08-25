//Muna Bhattarai
//CSE-3320
//Operating System-Malloc assignment


#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define ALIGN4(s)         (((((s) - 1) >> 2) << 2) + 4)
#define BLOCK_DATA(b)     ((b) + 1)
#define BLOCK_HEADER(ptr) ((struct _block *)(ptr) - 1)

static int atexit_registered = 0;
static int num_mallocs       = 0;
static int num_frees         = 0;
static int num_reuses        = 0;
static int num_grows         = 0;
static int num_splits        = 0;
static int num_coalesces     = 0;
static int num_blocks        = 0;
static int num_requested     = 0;
static int max_heap          = 0;

/*
 *  \brief printStatistics
 *
 *  \param none
 *
 *  Prints the heap statistics upon process exit.  Registered
 *  via atexit()
 *
 *  \return none
 */
void printStatistics( void )
{
  printf("\nheap management statistics\n");
  printf("mallocs:\t%d\n", num_mallocs );
  printf("frees:\t\t%d\n", num_frees );
  printf("reuses:\t\t%d\n", num_reuses );
  printf("grows:\t\t%d\n", num_grows );
  printf("splits:\t\t%d\n", num_splits );
  printf("coalesces:\t%d\n", num_coalesces );
  printf("blocks:\t\t%d\n", num_blocks );
  printf("requested:\t%d\n", num_requested );
  printf("max heap:\t%d\n", max_heap );
}

struct _block 
{
   size_t  size;         /* Size of the allocated _block of memory in bytes */
   struct _block *next;  /* Pointer to the next _block of allocated memory   */
   bool   free;          /* Is this _block free?                            */
   char   padding[3];    /* Padding: IENTRTMzMjAgU3ByaW5nIDIwMjM            */
};


struct _block *heapList = NULL; /* Free list to track the _blocks available */

/*
 * \brief findFreeBlock
 *
 * \param last pointer to the linked list of free _blocks
 * \param size size of the _block needed in bytes 
 *
 * \return a _block that fits the request or NULL if no free _block matches
 *
 * \TODO Implement Next Fit
 * \TODO Implement Best Fit
 * \TODO Implement Worst Fit
 */
struct _block *findFreeBlock(struct _block **last, size_t size) 
{
   struct _block *curr = heapList;

#if defined FIT && FIT == 0
   /* First fit */
   //
   // While we haven't run off the end of the linked list and
   // while the current node we point to isn't free or isn't big enough
   // then continue to iterate over the list.  This loop ends either
   // with curr pointing to NULL, meaning we've run to the end of the list
   // without finding a node or it ends pointing to a free node that has enough
   // space for the request.
   // 
   while (curr && !(curr->free && curr->size >= size)) 
   {
      *last = curr;
      curr  = curr->next;
   }
#endif

// \TODO Put your Best Fit code in this #ifdef block
#if defined BEST && BEST == 0
   /** \TODO Implement best fit here */
   struct _block *bestFitBlock = NULL;
   struct _block *bestFitPrev = NULL;
   size_t smallestBlock = (size_t)-1;

   while (curr)
   {
      if (curr->free && curr->size >= size && curr->size < smallestBlock)  //Iterate to find the smallest size block available that fits the demand
      {
         bestFitBlock = curr;
         bestFitPrev = *last;
         smallestBlock = curr->size;
      }
      *last = curr;
      curr = curr->next;
   }

   if (bestFitBlock) // when suitable block found, use pointer to indicate the block
   {
      *last = bestFitPrev;
      return bestFitBlock;
   }
#endif

// \TODO Put your Worst Fit code in this #ifdef block
#if defined WORST && WORST == 0
   /** \TODO Implement worst fit here */
   struct _block *worstFitBlock = NULL;
   struct _block *worstFitPrev = NULL;
   size_t largestBlock = 0; //Largest Block size available 

   while (curr)
   {
      if (curr->free && curr->size >= size && curr->size > largestBlock)   //Iterate to find the largest size block available
      {
         size_t currDiff = curr->size - size;
         if(currDiff > largestBlock)
         {
            largestBlock = currDiff;
            worstFitBlock = curr;
            worstFitPrev = *last;
            
         } 
      }
      *last = curr;
      curr = curr->next;
   }

   if (worstFitBlock)   //when suitable block found, use pointer to indicate the block
   {
      *last = worstFitPrev;
      return worstFitBlock;
   }
#endif

// \TODO Put your Next Fit code in this #ifdef block
#if defined NEXT && NEXT == 0
   /** \TODO Implement next fit here */
   //Next fit always start from the last allocated block 
  if(heapList)
   {
      curr = heapList;
   } 
   //Similar to the first Fit
    while (curr && !(curr->free && curr->size >= size)) 
   {
      *last = curr;
      curr  = curr->next;
   }

   heapList = curr;

#endif

   return curr;
}

/*
 * \brief growheap
 *
 * Given a requested size of memory, use sbrk() to dynamically 
 * increase the data segment of the calling process.  Updates
 * the free list with the newly allocated memory.
 *
 * \param last tail of the free _block list
 * \param size size in bytes to request from the OS
 *
 * \return returns the newly allocated _block of NULL if failed
 */
struct _block *growHeap(struct _block *last, size_t size) 
{
   /* Request more space from OS */
   struct _block *curr = (struct _block *)sbrk(0);
   struct _block *prev = (struct _block *)sbrk(sizeof(struct _block) + size);

   assert(curr == prev);

   /* OS allocation failed */
   if (curr == (struct _block *)-1) 
   {
      return NULL;
   }

   /* Update heapList if not set */
   if (heapList == NULL) 
   {
      heapList = curr;
   }

   /* Attach new _block to previous _block */
   if (last) 
   {
      last->next = curr;
   }

   /* Update _block metadata:
      Set the size of the new block and initialize the new block to "free".
      Set its next pointer to NULL since it's now the tail of the linked list.
   */
   curr->size = size;
   curr->next = NULL;
   curr->free = false;
   num_blocks++;
   max_heap += size + sizeof( struct _block);

   return curr;
}

/*
 * \brief malloc
 *
 * finds a free _block of heap memory for the calling process.
 * if there is no free _block that satisfies the request then grows the 
 * heap and returns a new _block
 *
 * \param size size of the requested memory in bytes
 *
 * \return returns the requested memory allocation to the calling process 
 * or NULL if failed
 */
void *malloc(size_t size) 
{

   num_mallocs++;

   if( atexit_registered == 0 )
   {
      atexit_registered = 1;
      atexit( printStatistics );
   }

   /* Align to multiple of 4 */
   size = ALIGN4(size);

   /* Handle 0 size */
   if (size == 0) 
   {
      return NULL;
   }

   /* Look for free _block.  If a free block isn't found then we need to grow our heap. */

   struct _block *last = heapList;
   struct _block *next = findFreeBlock(&last, size);

   /* TODO: If the block found by findFreeBlock is larger than we need then:
            If the leftover space in the new block is greater than the sizeof(_block)+4 then
            split the block.
            If the leftover space in the new block is less than the sizeof(_block)+4 then
            don't split the block.
   */

   
   /* Could not find free _block, so grow heap */
   if (next == NULL) 
   {
      next = growHeap(last, size);
      num_grows++;
   }

   /* Could not find free _block or grow heap, so just return NULL */
   if (next == NULL) 
   {
      return NULL;
   }

   if (next->size > size + sizeof(struct _block) + 4) 
   { 
      /* Split the block */ 
      struct _block *split_block = (struct _block *)((char *)next + sizeof(struct _block) + size);
      split_block->size = next->size - size - sizeof(struct _block);
      split_block->next = next->next;
      split_block->free = true;

      next->size = size;
      next->next = split_block;

      num_splits++; // Increment splits counter
   } 
   
   /* Mark _block as in use */
   next->free = false;
   num_reuses++;
   num_requested += size;

   /* Return data address associated with _block to the user */
   return BLOCK_DATA(next);
}

/*
 * \brief free
 *
 * frees the memory _block pointed to by pointer. if the _block is adjacent
 * to another _block then coalesces (combines) them
 *
 * \param ptr the heap memory to free
 *
 * \return none
 */
void free(void *ptr) 
{
   if (ptr == NULL) 
   {
      return;
   }

   /* Make _block as free */
   struct _block *curr = BLOCK_HEADER(ptr);
   assert(curr->free == 0);
   curr->free = true;
   num_frees++;

   /* TODO: Coalesce free _blocks.  If the next block or previous block 
            are free then combine them with this block being freed.
   */

/* Coalesce free _blocks */
   struct _block *next_block = curr->next;
   struct _block *prev_block = NULL;

   // Check if the next block is free and merge with the current block
   if (next_block && next_block->free) 
   {
      curr->size += sizeof(struct _block) + next_block->size;
      curr->next = next_block->next;
      num_coalesces++; // Increment coalesces counter
   }

// Iterate through the heap to find the previous block
   struct _block *heap_ptr = heapList;
   while (heap_ptr && heap_ptr != curr) 
   {
      prev_block = heap_ptr;
      heap_ptr = heap_ptr->next;
   }

   // Check if the previous block is free and merge with the current block
   if (prev_block && prev_block->free) 
   {
      prev_block->size += sizeof(struct _block) + curr->size;
      prev_block->next = curr->next;
      num_coalesces++; // Increment coalesces counter
   }
   
}

void *calloc( size_t nmemb, size_t size )
{
   // \TODO Implement calloc
   size_t total_size = nmemb * size;
   void *ptr = malloc(total_size);
   if (ptr != NULL)
   {
      memset(ptr, 0, total_size);
   }
   //return ptr;
   return NULL;
}

void *realloc( void *ptr, size_t size )
{
   // \TODO Implement realloc
   if (ptr == NULL)
   {
      return malloc(size);
   }

   if (size == 0)
   {
      free(ptr);
      return NULL;
   }

   struct _block *block = BLOCK_HEADER(ptr);
   size_t old_size = block->size;

   if (old_size >= size)
   {
      // The current block has enough space to hold the new size.
      // We don't need to allocate a new block.
      num_reuses++;
      return ptr;
   }

   void *new_ptr = malloc(size);
   num_requested++;
   if (new_ptr != NULL)
   {
      memcpy(new_ptr, ptr, old_size);
      free(ptr);
   }

   //return new_ptr;
   return NULL;
}



/* vim: IENTRTMzMjAgU3ByaW5nIDIwMjM= -----------------------------------------*/
/* vim: set expandtab sts=3 sw=3 ts=6 ft=cpp: --------------------------------*/