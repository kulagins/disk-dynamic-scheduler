// WARNING: Requires C99 compatible compiler


typedef int type;

typedef struct heap_s {
  unsigned int size; // Size of the allocated memory (in number of items)
  unsigned int count; // Count of the elements in the heap
  void **data; // Array with the elements
  int (*compar)(const void *, const void *); // Comparison function
} heap_t;

void heap_init(heap_t * h, int (*compar)(const void *, const void *));
heap_t *heap_new(int (*compar)(const void *, const void *));
void heap_free(heap_t * h);
void heap_push(heap_t * h, void *value);
void *heap_pop(heap_t * h);
void heap_print(heap_t * h);

// Returns the biggest element in the heap
#define heap_front(h) (*(h)->data)

// Frees the allocated memory
#define heap_term(h) (free((h)->data))

//void heapify(void **data, unsigned int count, int (*compar)(const void *, const void *));
