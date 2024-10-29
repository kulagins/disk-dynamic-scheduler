//#include <gvc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <graphviz/cgraph.h>
#include "fifo.hpp"

fifo_t *fifo_new(void){
  fifo_t *f= (fifo_t*)malloc(sizeof(fifo_t));
  f->size = 100;
  f->elements = (void**)calloc(f->size,sizeof(void*));
  f->head=0;
  f->tail=0;
  return f;
}

int fifo_is_empty(fifo_t *f) {
  return (f->head == f->tail);
}

int fifo_size(fifo_t *f) {
  return (f->tail + f->size - f->head) % f->size;
}


void *fifo_read(fifo_t *f) {
  void *elt = f->elements[f->head];
  if (f->head == f->tail) {
    fprintf(stderr,"fifo %p error: Attempt to get element from empty fifo\n",(void*)f);
    exit(2);
  }
  f->head = (f->head + 1) % f->size;
  return elt;
}

void fifo_write(fifo_t *f, void* elt) {
  if ((f->tail+1)%(f->size) == f->head) { // head==tail represents empty fifo, so there will always be one free space
    int old_size=f->size;
    f->size = f->size * 2;
    //    fprintf(stderr,"realloc to size %d (h:%d t:%d)\n",f->size, f->head, f->tail);
	
    f->elements = realloc(f->elements, f->size*sizeof(void*));
    if(f->tail < f->head) {
      // copy elements[0...tail-1] to [old_size.. old_size+tail-1]
      // so that new fifo is located on [head..old_size+tail-1]
      for(int i=0; i<f->tail; i++) {
	f->elements[old_size+i] = f->elements[i];
	f->elements[i] = NULL;
      }
      f->tail = old_size + f->tail;
    }
  }
  f->elements[f->tail] = elt;
  f->tail = (f->tail + 1) % (f->size);
}
  

void fifo_free(fifo_t *f) {
  free(f->elements);
  free(f);
}

int fifo_is_in(fifo_t *f, void *item) {
// fifo with elements[(head % size)...((tail-1)%size)]
  if (f->head <= f->tail) { // elements in f[head]...f[tail-1]
    for(int i=f->head; i<f->tail; i++) {
      if (f->elements[i]==item) {
	return 1;
      }
    }
    return 0;
  } else { // elements in f[0] ...f[tail-1] and f[head]...f[size-1]
    for(int i=0; i<f->tail; i++) {
      if (f->elements[i]==item) {
	return 1;
      }
    }
    for(int i=f->head; i<f->size; i++) {
      if (f->elements[i]==item) {
	return 1;
      }
    }
    return 0;

  }
}

