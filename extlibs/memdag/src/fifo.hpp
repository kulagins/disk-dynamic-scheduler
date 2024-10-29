
#ifndef FIFO_H
#define FIFO_H

// FIFO with elements[(head % size)...((tail-1)%size)]
typedef struct fifo {
  void **elements; 
  int head, tail, size;
} fifo_t;

fifo_t *fifo_new(void);
int     fifo_is_empty(fifo_t *f);
int     fifo_size(fifo_t *f);
void   *fifo_read(fifo_t *f);
void    fifo_write(fifo_t *f, void* elt);
void    fifo_free(fifo_t *f);
int     fifo_is_in(fifo_t *f, void *item);
#endif
