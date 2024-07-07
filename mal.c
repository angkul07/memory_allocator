#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

typedef char ALIGN[16];

// union size is larger than its member so it make sure that end of the header is memory aligned
union header{
    struct {
        // storing the size of block and if it is free or not in block
        size_t size;
        unsigned is_free;
        // using linked list to keep track of the memory allocated by our malloc
        union header *next;
    } s;

    // allocator will be aligned to 16 bytes i.e force the header to be aligned to 16 bytes
    ALIGN stub;
};
typedef union header header_t;

// to keep track of list
header_t *head= NULL, *tail=NULL;

// to prevent two or more threads from concurrently accessing memory, we put a basic locking mechanism
pthread_mutex_t global_malloc_lock;

header_t *get_free_block(size_t size){
    header_t *curr = head;
    while(curr){
        if (curr -> s.is_free && curr -> s.size >= size){
            return curr;
        }
        curr = curr -> s.next;
    }
    return NULL;
}

// determine if the block is free or not and if yes, then we can realse it to the OS
void free(void *block){
    header_t *header, *tmp;
    void *programbreak;

    if(!block){return;}

    pthread_mutex_lock(&global_malloc_lock);
    // We cast block to a header pointer type and move it behind by 1 unit
    header = (header_t*)block - 1;

    // gives the current value of program break
    programbreak = sbrk(0);
    /* check if the block to be freed at the end of the head or not.
     if it is, then we can shrink heap size and realase the memory to OS */
    if((char*)block + header->s.size == programbreak){
        if(head == tail){head = tail = NULL;}
        else{
            tmp = head;
            while(tmp){
                if(tmp -> s.next == tail){
                    tmp -> s.next = NULL;
                    tail = tmp;
                }
                tmp = tmp->s.next;
            }
        }
        /*sbrk with negative argument decrements the program break
        So memory is released by the program to OS*/
        sbrk(0 - header->s.size - sizeof(header_t));
        pthread_mutex_unlock(&global_malloc_lock);
        return;
    }
    header -> s.is_free = 1;
    pthread_mutex_unlock(&global_malloc_lock);
}

void *malloc(size_t size){
    size_t total_size;
    void *block;
    header_t *header;
    if(!size){return NULL;}
    pthread_mutex_lock(&global_malloc_lock);

    header = get_free_block(size);
    if(header){
        /*found a free block to accomodate requested memory.*/
        header -> s.is_free=0;
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }

    total_size = sizeof(header_t) + size;
    block = sbrk(total_size);
    if (block == (void*)-1){
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }

    header = block;
    header -> s.size = size;
    header -> s.is_free = 0;
    header -> s.next = NULL;

    if(!head){head = header;}
    if(tail){tail -> s.next = header;}
    tail = header;
    pthread_mutex_unlock(&global_malloc_lock);
    return (void*)(header + 1);
}

// allocates memory for an array of num elements of nsize bytes each and returns a points to the allocated memory
void *calloc(size_t num, size_t nsize){
    size_t size;
    void *block;
    if(!num || !nsize){return NULL;}
    size = num * nsize;
    // check mul overflow
    if(nsize != size/num){return NULL;}
    block = malloc(size);
    if(!block){return NULL;}
    // memset: fill memory with a constant byte
    memset(block, 0, size);
    return block;
}

// change size of the given memory block
void *realloc(void *block, size_t size){
    // if block has the requested size, then do nothing
    header_t *header;
    void *ret;

    // if it don't have the requested size, call malloc() to get a block of the request size
    if(!block || !size){return malloc(size);}

    header = (header_t*)block - 1;

    if (header -> s.size >= size){return block;}

    // memcpy: copy memory area
    ret = malloc(size);
    if (ret){
        memcpy(ret, block, header -> s.size);
        free(block);
    }
    return ret;
}

/* A debug function to print the entire link list */
void print_mem_list()
{
	header_t *curr = head;
	printf("head = %p, tail = %p \n", (void*)head, (void*)tail);
	while(curr) {
		printf("addr = %p, size = %zu, is_free=%u, next=%p\n",
			(void*)curr, curr->s.size, curr->s.is_free, (void*)curr->s.next);
		curr = curr->s.next;
	}
}

int main(){
    // Initialize the mutex
    pthread_mutex_init(&global_malloc_lock, NULL);

    printf("Testing custom memory allocator\n\n");

    // Allocate some memory
    int *arr1 = (int*)malloc(5 * sizeof(int));
    int *arr2 = (int*)calloc(3, sizeof(int));
    
    // Print the memory list
    printf("After allocations:\n");
    print_mem_list();
    
    // Free some memory
    free(arr1);
    
    // Print the memory list again
    printf("\nAfter freeing arr1:\n");
    print_mem_list();
    
    // Reallocate some memory
    arr2 = (int*)realloc(arr2, 5 * sizeof(int));
    
    // Print the memory list one more time
    printf("\nAfter reallocation:\n");
    print_mem_list();
    
    // Clean up
    free(arr2);

}
