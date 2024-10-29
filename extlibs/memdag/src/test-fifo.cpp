#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include "fifo.hpp"


void test_fifo(void) {
  fifo_t *f=fifo_new();

  for(int i=0; i<80; i++) {
    fifo_write(f,(void*)(uintptr_t)i);
  }
  for(int i=0; i<80; i++) {
    assert(fifo_is_in(f,(void*)(uintptr_t)i));
  }
  for(uintptr_t i=0; i<70; i++) {
    uintptr_t j= (uintptr_t)fifo_read(f);
    assert(i==j);
    assert(fifo_is_in(f,(void*)(uintptr_t)i)==0);
  }



  //  fprintf(stderr,"\n");
  for(int i=80; i<300; i++) {
    //    fprintf(stderr,"i:%d t:%d\n",i,f->tail);
    assert(fifo_is_in(f,(void*)(uintptr_t)i)==0);
    fifo_write(f,(void*)(uintptr_t)i);
    //fprintf(stderr,"head:%d  tail:%d size:%d\n",f->head, f->tail, f->size);
    assert(fifo_is_in(f,(void*)(uintptr_t)i));
  }
  for(uintptr_t i=70; i<300; i++) {
    uintptr_t j= (uintptr_t)fifo_read(f);
    assert(i==j);
    assert(fifo_is_in(f,(void*)(uintptr_t)i)==0);   
  }

  fifo_free(f);
}


int main(int argc, char **argv) {

  test_fifo();
  fprintf(stdout,"OK\n");
  return 0;
}
