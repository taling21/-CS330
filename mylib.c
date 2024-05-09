#include <stddef.h>
#include <sys/mman.h>
#include <stdio.h>
#include <math.h>

// Metadata structure for free memory chunks
typedef struct Metadata {
    size_t size;
    struct Metadata* next;
    struct Metadata* prev;
} Metadata;


// Free memory list
static Metadata* head = NULL;

void* memalloc(unsigned long size) {
    if (size == 0) {
        return NULL;
    }

    size_t aligned_size = (size + sizeof(size_t) + 7) & ~7; // Align to 8 bytes
    if(aligned_size < 24) aligned_size = 24;  // padding to make it of atleast 24 size 
    Metadata* current = head;
	Metadata* temp1, *temp2;
	size_t temp_size;

    while (current) {
        //printf("inside\n");
        if (current->size >= aligned_size) {
            size_t remaining_size = current->size - aligned_size;   // size which would be left after taking out the requred size from the chunk 

            if (remaining_size < 24) {
                // Remove the whole chunk from the free list, and adjust prev and next pointers of surrounding chunks
                if (current->prev) {
                    current->prev->next = current->next;
                } else {
                    head = current->next;
                }

                if (current->next) {
                    current->next->prev = current->prev;
                }
				*((size_t*)((char*)current)) = current->size;
                return (char*)current + sizeof(size_t);
            } else {
                // Split the chunk
				
				if(head != current) {
                    temp1 = current->next;
                    temp2 = current->prev;
                    Metadata* new_chunk = (Metadata*)((char*)current + aligned_size);  // new chunk after split 
                    new_chunk->size = remaining_size;
                    new_chunk->next = head;
                    new_chunk->prev = NULL;

                    if (temp1) {
                        temp1->prev = temp2;
                    }
                    if(temp2) temp2->next = temp1;
                    if(head) head->prev = new_chunk;
                    head = new_chunk;
                }
                else {
                    temp1 = current->next;
	                Metadata* new_memory_chunk = (Metadata*)((char*)current + aligned_size);  // after allocating the req memory,a free chunk denoting next free memory 
				// adjusting pointers , here only head is changing. 
                    new_memory_chunk->size = remaining_size;
                    new_memory_chunk->next = temp1;
                    new_memory_chunk->prev = NULL;
                    
                    if (temp1) {
                        temp1->prev = new_memory_chunk;
                    }
                    head = new_memory_chunk;

                }

                *((size_t*)((char*)current)) = aligned_size;
				

                return ((char*)current) + sizeof(size_t);  // 8 bytes store the size, return the address after that 
            }
        }

        current = current->next;
    }

    // No suitable free chunk found, request memory from the OS, in multiples of 4mb if requested > 4mb
   
    size_t mmap_size = (aligned_size > 4 * 1024 * 1024) ? (4 * 1024 * 1024)*((aligned_size + 4 * 1024 * 1024 - 1)/ (4 * 1024 * 1024)): 4 * 1024 * 1024;
    
	
    Metadata* new_chunk = (Metadata*)mmap(NULL, (unsigned long) mmap_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); // requesting new memory from mmap
	
    if (new_chunk == MAP_FAILED) {
		
        return NULL;
    }
	

    new_chunk->size = mmap_size;  
    new_chunk->next = NULL;
    new_chunk->prev = NULL;

    if (head) {
        head->prev = new_chunk;
        new_chunk->next = head;
    }

    head = new_chunk;   // head is now at the newly formed chunk

    current = head;   // now, we are at the head, and rest code is similar to the case of removing full chunk from the list 
	
	
	size_t remaining_size = current->size - aligned_size;

    if(remaining_size < 24){
        head = head->next;
        *((size_t*)((char*)current)) = current->size;
    }
    else {
	temp1 = current->next;
	
	Metadata* new_memory_chunk = (Metadata*)((char*)current + aligned_size);  // after allocating the req memory,a free chunk denoting next free memory 
				// adjusting pointers , here only head is changing. 
                new_memory_chunk->size = remaining_size;
                new_memory_chunk->next = temp1;
                new_memory_chunk->prev = NULL;
                
                if (temp1) {
                    temp1->prev = new_memory_chunk;
                }
			
                head = new_memory_chunk;
                *((size_t*)((char*)current)) = aligned_size;
    }
						
                return ((char*)current) + sizeof(size_t);

}

int memfree(void* ptr) {
    if (!ptr) {
        return -1;
    }

    Metadata* metadata = (Metadata*)((char*)ptr - sizeof(size_t));

    // set the size of free chunk
    metadata->next = NULL;
    metadata->size = *((size_t*)((char*)metadata)); 
    metadata->prev = NULL;

    // Find the entry in the free list just before ptr
  
    Metadata* current = head;
    while (current) {
    //printf("gg %p %p\n ", current, head);
    if ((char*)current + current->size == (char*)metadata) {
        // Coalesce with left chunk
        current->size += metadata->size;
        if(current->prev) {
            current->prev->next = current->next;
        }
        else head = current->next;
        if(current->next) {
            current->next->prev = current->prev;
        }
        metadata = current;
        break;
    }
   
    current = current->next;
    }
    
    current = head;
    while (current) {
        // coalesce with right chunk, if it exists
        if ((char*)metadata + metadata->size == (char*)current) {
            metadata->size += current->size;
            if (current->next) {
                current->next->prev = current->prev;
            }

            if (current->prev) {
                current->prev->next = current->next;
            } 
            else head = current->next;
            break;
        }

        current = current->next;
    }
    if(metadata != head){
    metadata->next = head;
    if(head) {
        head->prev = metadata;
       // metadata->next = head;
    }
    }
    head = metadata;  
    return 0;
}



