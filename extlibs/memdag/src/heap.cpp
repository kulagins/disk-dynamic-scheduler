// WARNING: Requires C99 compatible compiler

//#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "heap.hpp"


static const unsigned int base_size = 4;

//#define CMP(a, b) ((a.time) < (b.time) || (((a.time)==(b.time)) && (a.proc >= b.proc))) 
//#define CMP(a, b) ((a.time) <= (b.time))


// Prepares the heap for use
void heap_init(heap_t * h,  int (*compar)(const void *, const void *))
{
  h->size = base_size;
  h->count = 0;
  h->data = malloc(sizeof(void*) * base_size);
  h->compar = compar;
  /* *h = (heap_t){ */
  /*   .size = base_size, */
  /*   .count = 0, */
  /*   .data = malloc(sizeof(void*) * base_size) */
  /*   .compar = compar */
  /* }; */
  assert(h->data); // Exit if the memory allocation fails
}

heap_t *heap_new(int (*compar)(const void *, const void *)) {
  heap_t *h = (heap_t*) calloc(1,sizeof(heap_t));
  heap_init(h, compar);
  return h;
}

void heap_free(heap_t *h) {
  free(h->data);
  free(h);
}

// Inserts element to the heap
void heap_push(heap_t * h, void *value)
{
  unsigned int index, parent;
  
  // Resize the heap if it is too small to hold all the data
  if (h->count == h->size)
    {
      h->size <<= 1;
      h->data = realloc(h->data, sizeof(void*) * h->size);
      assert(h->data);
    }
  
  // Find out where to put the element and put it
  for(index = h->count++; index; index = parent)
    {
      parent = (index - 1) >> 1;
      if ((*(h->compar))(h->data[parent], value)) break;
      h->data[index] = h->data[parent];
    }
  h->data[index] = value;
}

// Removes the biggest element from the heap
void *heap_pop(heap_t * h)
{
  unsigned int index, swap, other;
  // Take the value to return
  void *return_value = heap_front(h);
  
  // Remove the biggest element
  void *temp = h->data[--h->count];
  
  // Resize the heap if it's consuming too much memory
  if ((h->count <= (h->size >> 2)) && (h->size > base_size))
    {
      h->size >>= 1;
      h->data = realloc(h->data, sizeof(void*) * h->size);
      assert(h->data);
    }
  
  // Reorder the elements
  for(index = 0; 1; index = swap)
    {
      // Find the child to swap with
      swap = (index << 1) + 1;
      if (swap >= h->count) break; // If there are no children, the heap is reordered
      other = swap + 1;
      if ((other < h->count) && ((*(h->compar))(h->data[other], h->data[swap]))) swap = other;
      if ((*(h->compar))(temp, h->data[swap])) break; // If the bigger child is less than or equal to its parent, the heap is reordered
      
      h->data[index] = h->data[swap];
    }
  h->data[index] = temp;
  return return_value;
}

/* void heap_print(heap_t *h) { */
/*   int i; */
/*   fprintf(stderr, "heap with %d elements (size:%d):", h->count, h->size); */
/*   for(i=0; i<h->count; i++) { */
/*     fprintf(stderr, " (%.2f, %d)", h->data[i].time, h->data[i].proc); */
/*   } */
/*   fprintf(stderr,"\n"); */
/* } */

// Heapifies a non-empty array
/* void heapify(void **data, unsigned int count, int (*compar)(const void *, const void *)) */
/* { */
/*   unsigned int item, index, swap, other; */
/*   void *temp; */
  
/*   // Move every non-leaf element to the right position in its subtree */
/*   item = (count >> 1) - 1; */
/*   while (1) */
/*     { */
/*       // Find the position of the current element in its subtree */
/*       temp = data[item]; */
/*       for(index = item; 1; index = swap) */
/* 	{ */
/* 	  // Find the child to swap with */
/* 	  swap = (index << 1) + 1; */
/* 	  if (swap >= count) break; // If there are no children, the current element is positioned */
/* 	  other = swap + 1; */
/* 	  if ((other < count) && ((*compar)(data[other], data[swap]))) swap = other; */
/* 	  if ((*compar)(temp, data[swap])) break; // If the bigger child is smaller than or equal to the parent, the heap is reordered */
	  
/* 	  data[index] = data[swap]; */
/* 	} */
/*       if (index != item) data[index] = temp; */
      
/*       if (!item) return; */
/*       --item; */
/*     } */
/* } */
