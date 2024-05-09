#include <types.h>
#include <mmap.h>
#include <fork.h>
#include <v2p.h>
#include <page.h>

/* 
 * You may define macros and other helper functions here
 * You must not declare and use any static/global variables 
 * */
#define PAGE_SIZE 4096

struct vm_area* alloc_vm_area(u64 addr, int length, int prot) {
    struct vm_area *vma = (struct vm_area *)os_alloc(sizeof(struct vm_area));
    vma->vm_start = addr;
    vma->vm_end = addr + length;
    vma->access_flags = prot;
    vma->vm_next = NULL;
   // printk("allocating vm_area %x, %x, %x\n", addr, length, prot);
    return vma;
}

struct vm_area* merge(struct vm_area* left, struct vm_area* right){
    if(left->vm_start == MMAP_AREA_START) return NULL;
    if(left->vm_end == right->vm_start && left->access_flags == right->access_flags){
        left->vm_end = right->vm_end;
        left->vm_next = right->vm_next;
        os_free(right, sizeof(struct vm_area));
        stats->num_vm_area--;  // decrement counter only if merge successful
        return left;
    }
    return NULL;
}

void initialize_page_table(u64* ptr){
    int i;
    for(i=0;i<512;i++){
        ptr[i] = 0;
    }
    return ;
}


void page_table_walk_permission(struct exec_context *current, u64 addr , int permit_bit){
    int i;
    u32 pfn;
    u64* ptr;
    pfn = current->pgd;
    int flag = 1;
    int original_bit;
    for(i=0;i<4;i++){

        ptr = (u64*)osmap(pfn);
        ptr = ptr + ((addr >> (12 + 9*(3-i))) & 0x1FF);
        if(((*ptr)&1) == 0 ) {
         //   ptr = temp;
            flag = 0;
            break; // not mapped afterwards, deal with this
        }
        pfn = (*ptr) >> 12;
        
        if(i<3) *ptr = (*ptr) | (permit_bit << 3);  // not final level, so change only from read to write
        //*ptr = *ptr & (0xFFFFFFFFFFFFFFF7 | (permit_bit << 3));
       
    }
    if(flag == 0) return ;
    original_bit = (*ptr)&(0x8);
    if(original_bit == permit_bit) return ;

    if(permit_bit == 0 ){
    *ptr = ((*ptr) & 0xFFFFFFFFFFFFFFF7) | (permit_bit << 3); // hard setting
    }
    else {
        if(get_pfn_refcount(pfn) == 1) *ptr = ((*ptr) & 0xFFFFFFFFFFFFFFF7) | (permit_bit << 3);
        else {
            // do nothing(handle later in cow) , or allocate a new one 
            ;
        }
    }
    return ;
}
void page_table_walk_deletion(struct exec_context *current, u64 addr){ 
    int i;
    u32 pfn;
    u64* ptr;
    pfn = current->pgd;
    int flag = 1;
    for(i=0;i<4;i++){
        ptr = (u64*)osmap(pfn);
        ptr = ptr + ((addr >> (12 + 9*(3-i))) & 0x1FF);
        if(((*ptr)&1) == 0 ) {
         //   ptr = temp;
            flag = 0;
            break; // not mapped afterwards, deal with this
        }
        pfn = (*ptr) >> 12;
    }
    if(flag == 0) return ;
    if(get_pfn_refcount(pfn) > 0) {
       // printk("before putting in unmap process = %d\n", current->pid);
        put_pfn(pfn);
       // printk("after in unmap\n");
    }
    if(get_pfn_refcount(pfn) == 0) 
    {
        //printk("here pfn for freeing = %x\n", pfn);
        os_pfn_free(USER_REG, pfn);
        //printk(" freed the pfn \n");
    }
    
    *ptr = 0; // made entirely as 0
    return ;
}

void page_table_walk_insertion(struct exec_context* current, u32 start_pfn, u64 addr){

    //printk("page table walk insertion %x\n", addr);
    int i;
    u32 pfn_old;
    u64* ptr_old;
    u32 pfn_new;
    u64* ptr_new;
    u32 target;
    pfn_new = start_pfn;
    pfn_old = current->pgd;
    int permit_bit;
    for(i=0;i<4;i++){
        ptr_new = (u64*)osmap(pfn_new);
        ptr_old = (u64*)osmap(pfn_old);
        
        ptr_new = ptr_new + ((addr >> (12 + 9*(3-i))) & 0x1FF);
        ptr_old = ptr_old + ((addr >> (12 + 9*(3-i))) & 0x1FF);
        
        if(((*ptr_old)&1) == 0 ) {
          
           *ptr_new = *ptr_old; // will be invalid
           //printk("invalid address = %x, i = %d \n", addr);
            break; 
        }

        if(((*ptr_new)&1) == 0 ) { // if its an invalid entry in new page table
        permit_bit = ((*ptr_old) >> 3) & (0x1); // permit bit is unnecessary now
        
        if(i < 3)
            {
                pfn_new = os_pfn_alloc(OS_PT_REG);
                //*ptr_new = ((*ptr_new)) | ((*ptr_old)&(0xFFF)); //copy the last 12 bits from parent pte
                *ptr_new =  ((*ptr_old) & 0xFFF) | (pfn_new << 12) | 1 | (permit_bit << 3) | (0x10);
                initialize_page_table(osmap(pfn_new));

            }
        else {

            target = (*ptr_old) >> 12;
            *ptr_old = *ptr_old & (0xFFFFFFFFFFFFFFF7); // made permission bit as 0, only read permission set

            *ptr_new =  ((*ptr_old) & 0xFFF) | (target << 12) | 1 | 0 | (0x10);  // only read permission at last, and same pfn as parent
            
            get_pfn(target);
            
            
        }
        }
            pfn_new = (*ptr_new) >> 12;
            pfn_old = (*ptr_old) >> 12;
    }
    return ;
}

int page_table_walk_cow(struct exec_context* current, u64 addr, int permit_bit){
  
    u32 pfn;
    pfn = current->pgd;
    int i;
    u64* ptr;
    u32 new_pfn;
    for(i=0;i<4;i++){

        ptr = (u64*)osmap(pfn);
        ptr = ptr + ((addr >> (12 + 9*(3-i))) & 0x1FF);
        if(((*ptr)&1) == 0 ) {
         //   ptr = temp;
            
            return -1; // not mapped afterwards, error
        }
        pfn = (*ptr) >> 12;    
        if(i == 3){
            if(get_pfn_refcount(pfn) == 1 ){
             //   printk( " cow single reference case ref count = %d\n", get_pfn_refcount(pfn));
                *ptr = ((*ptr) & 0xFFFFFFFFFFFFFFF7)|(permit_bit << 3);
            }
            else{
            
            new_pfn = os_pfn_alloc(USER_REG);  
          //  printk(" new pfn in cow = %x, %d\n", new_pfn, current->pid);
            *ptr =  ((*ptr) & 0xFFF) | (new_pfn << 12) | 1 | (0x10);
            *ptr = ((*ptr) & 0xFFFFFFFFFFFFFFF7)|(permit_bit << 3);
            
            u64* temp = (u64*)osmap(new_pfn);
            memcpy(temp, osmap(pfn), PAGE_SIZE);
            if(get_pfn_refcount(pfn) > 0) {
             //   printk("before putting in cow%x\n", pfn);
                put_pfn(pfn);

            }
            }
        }
           
    }
    return 1;

}
struct vm_area* search(struct exec_context *current, u64 addr){
    struct vm_area *vma = current->vm_area;
    
    while (vma) {

        if (vma->vm_start <= addr && vma->vm_end > addr) {
            return vma;
        }
        vma = vma->vm_next;
    }
    return NULL;
}



void handle_faults(u64* ptr, u64 addr ,int i, int permit_bit){
    u32 new_pfn;
    while(i<3){
        new_pfn = os_pfn_alloc(OS_PT_REG);
        *ptr =  ((*ptr) & 0xFFF) | (new_pfn << 12) | 1 | (permit_bit << 3) | (0x10);

        
        ptr = (u64*)osmap(new_pfn);
        i++;
        initialize_page_table(ptr);
        ptr  = ptr + ((addr >> (12 + 9*(3-i))) & 0x1FF);
        
        
    }
    new_pfn = os_pfn_alloc(USER_REG);
    *ptr =  ((*ptr) & 0xFFF) | (new_pfn << 12) | 1 | (permit_bit << 3) | (0x10);
    
    return ;


}

void flush_tlb(){
    asm volatile(
        "mov %cr3, %rax;"
        "mov %rax, %cr3;"
    );
    return ;
}
void get_pages_to_free(struct exec_context *current, u64 addr, u64 end_addr){
    int i = 0;
    u64 temp ;
    temp = addr;
    while(temp < end_addr){
        page_table_walk_deletion(current, temp);
        temp += PAGE_SIZE;
    }
    flush_tlb();
    return ;

}

void get_pages_for_permission(struct exec_context *current, u64 addr, u64 end_addr, int permit_flags){
    int i = 0;
    u64 temp ;
    temp = addr;
    int permit_bit;
    if(permit_flags == (PROT_READ|PROT_WRITE)) permit_bit = 1;
    else permit_bit = 0;
   
    while(temp < end_addr){
        page_table_walk_permission(current, temp, permit_bit);
        temp += PAGE_SIZE;
    }
    flush_tlb();
    return ;

}

void get_pages_for_allocation(struct exec_context* current, u32 start_pfn){
    int i = 0;
    u64 temp ;
    int arr[4];
    arr[0] = MM_SEG_CODE;
    arr[1] = MM_SEG_RODATA;
    arr[2] = MM_SEG_DATA;
    arr[3] = MM_SEG_STACK;
    while(i<4){
        temp = current->mms[arr[i]].start;
       
        if(i<3) {while(temp < current->mms[arr[i]].next_free){
            
            
            page_table_walk_insertion(current, start_pfn, temp);
            temp += PAGE_SIZE;
        }
        }
        else {
            while(temp < current->mms[arr[i]].end){
            
            page_table_walk_insertion(current, start_pfn, temp);
            temp += PAGE_SIZE;
        }

        }
        i++;
    }
    
    struct vm_area *vma = current->vm_area;
   
   
    if(vma){
      
    vma = vma->vm_next; // first node always irrelevant
    
    while(vma){
        temp = vma->vm_start;
      
        while(temp < vma->vm_end){
            
            page_table_walk_insertion(current, start_pfn, temp);
            
            temp += PAGE_SIZE;
            
        }
        vma = vma->vm_next;
    }
    }
    flush_tlb(); // maybe necessary bcz protections changed for parent, check anyways
    
    return ;
}

void copy_linked_list(struct exec_context* current, struct exec_context* new_ctx){
    struct vm_area *vma = current->vm_area;
    struct vm_area *prev = NULL;
    struct vm_area *new_vma;
    while(vma){
        new_vma = alloc_vm_area(vma->vm_start, vma->vm_end - vma->vm_start, vma->access_flags);
        
        
        if(!new_vma) return ;
        new_vma->vm_next = NULL;
        if(prev == NULL) new_ctx->vm_area = new_vma;
        else prev->vm_next = new_vma;
        prev = new_vma;
        vma = vma->vm_next;
    }
   
    return ;
}
/**
 * mprotect System call Implementation.
 */
long vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{
    if (length <= 0 || !current) {
        return -EINVAL;  // Invalid arguments
    }
    if(prot != PROT_READ && prot != (PROT_READ | PROT_WRITE)) return -EINVAL; // Invalid protection flags
    length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // Calculate the end address based on the given address and length
    u64 end_addr = addr + length ;

    struct vm_area *vma = current->vm_area;
    if(!vma) return -EINVAL;  // no vm_area to mprotect

    while (vma) {
        if (vma->vm_end <= addr) {
            vma = vma->vm_next;
            continue;
        }

        if (vma->vm_start >= end_addr) {
            break;
        }

        // Check if the current VMA is within the range
        if (vma->vm_start >= addr && vma->vm_end <= end_addr) {
            // Update the access flags for the entire VMA
            get_pages_for_permission(current, vma->vm_start, vma->vm_end, prot);
            vma->access_flags = prot;
        } else {
            // Handle the case where the range partially overlaps with the VMA
            if (vma->vm_start < addr && vma->vm_end > end_addr) { // range completely inside this 
                // Adjust the end of the VMA
                
                get_pages_for_permission(current, addr, end_addr, prot);
                // Split the VMA into two parts
                if(vma->access_flags == prot) break; // no need to split
                struct vm_area *new_vma_1 = alloc_vm_area(addr, length, prot);
                struct vm_area *new_vma_2 = alloc_vm_area(end_addr, vma->vm_end - end_addr, vma->access_flags);
                if(!new_vma_1 || !new_vma_2) return -EINVAL;
                stats->num_vm_area += 2;  // increment counter
                new_vma_1->vm_next = new_vma_2;
                new_vma_2->vm_next = vma->vm_next;
                vma->vm_end = addr;
                vma->vm_next = new_vma_1;
                break;

        }
        else if(vma->vm_start < addr){  // range starts inside this
            
            if(vma->access_flags == prot) return 0;  // no need to split
            get_pages_for_permission(current , addr, vma->vm_end, prot);
            
            struct vm_area *new_vma = alloc_vm_area(addr, vma->vm_end - addr, prot);
            vma->vm_end = addr;
            if(!new_vma) return -EINVAL;
            stats->num_vm_area++;  // increment counter
            new_vma->vm_next = vma->vm_next;
            vma->vm_next = new_vma;
        }
        else if(vma->vm_end > end_addr){  // range ends inside this

            if(vma->access_flags == prot) break;  // no need to split
            get_pages_for_permission(current, vma->vm_start, end_addr, prot);
           
            
            struct vm_area *new_vma = alloc_vm_area(end_addr, vma->vm_end - end_addr, vma->access_flags);
            vma->vm_end = end_addr;
            if(!new_vma) return -EINVAL;
            stats->num_vm_area++;  // increment counter
            new_vma->vm_next = vma->vm_next;
            vma->vm_next = new_vma;
            vma->access_flags = prot;
            break;
        }
        }

        vma = vma->vm_next;
    }

    // Merge adjacent VMAs with the same protection flags
    struct vm_area *prev = NULL;
    vma = current->vm_area;
    while (vma && vma->vm_next) {
        // Merge the two VMAs
            if(!merge(vma, vma->vm_next)) vma = vma->vm_next;  // merge returns NULL if merge unsuccessful. Also, the left argument is the same. 
    }

    return 0; // Success
}

/**
 * mmap system call implementation.
 */

long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{   

     if (length <= 0 || length > (2 * 1024 * 1024) || !current) {
        return -EINVAL;  // Invalid arguments
    }

    if (flags != 0 && flags != MAP_FIXED) {
        return -EINVAL;  // Invalid flag
    }

    if(addr != 0 && ((addr < MMAP_AREA_START) || (addr > MMAP_AREA_END))) return -EINVAL; // invalid hint
    if(prot != PROT_READ && prot != (PROT_READ | PROT_WRITE)) return -EINVAL; // Invalid protection flags

    length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // If addr is NULL, find the lowest available address
  
    if(!current->vm_area){  // no dummy node, so add one
        struct vm_area *dummy = alloc_vm_area(MMAP_AREA_START,4096,0x0);
        if(!dummy) return -EINVAL;
        current->vm_area = dummy;
        stats->num_vm_area++;  // increment counter
    }


    if (addr == 0) {
        if(flags == MAP_FIXED) return -EINVAL;
        addr = MMAP_AREA_START;


        // Iterate through the list of VMAs to find the lowest available address
        
        struct vm_area *vma = current->vm_area;
        struct vm_area *prev = NULL;
        while (vma) {
            if (addr + length <= vma->vm_start) {
                break;
            }
            addr = vma->vm_end;
            prev = vma;
            vma = vma->vm_next;
            
        }
        // If the lowest available address is not in the MMAP area, return error
        if (addr + length > MMAP_AREA_END) {
            return -EINVAL;
        }
        struct vm_area *new_vma = alloc_vm_area(addr,length,prot);
        if(!new_vma) return -EINVAL;
        new_vma->vm_next = vma;
        prev->vm_next = new_vma;
        stats->num_vm_area++;  // increment counter

        merge(new_vma, new_vma->vm_next);
        unsigned long temp = new_vma->vm_start;
        merge(prev, new_vma);
        //if(merged != NULL) new_vma = merged;
        return temp;

    }

    // the case when addr is given as a hint
    struct vm_area *prev = NULL;
    struct vm_area *vma = current->vm_area;
    
    while (vma) {
        if (vma->vm_end <= addr) {
            prev = vma;
            vma = vma->vm_next;
            continue;
        }

        if (addr + length <= vma->vm_start) {
            // New mapping doesn't overlap with this VMA
            struct vm_area* new_vma = alloc_vm_area(addr,length,prot);
            if(!new_vma) return -EINVAL;
            new_vma-> vm_next = vma;
            prev->vm_next = new_vma;
            stats->num_vm_area ++; // incrementing

            merge(new_vma, new_vma->vm_next);
            unsigned long temporary = new_vma->vm_start;
            merge(prev, new_vma);
           // if(merged != NULL) new_vma = merged;
            return temporary;
        } 
        else {
            // Overlapping with this VMA
            if (flags == MAP_FIXED) {
                return -1; // MAP_FIXED flag is set, and address already mapped
            }
            return vm_area_map(current, 0,length, prot, flags );  // do like the case of no hint

        }
    }
    // case where hint address slot is empty
    struct vm_area* new_vma = alloc_vm_area(addr,length,prot);
    if(!new_vma) return -EINVAL;
    prev->vm_next = new_vma;
    stats->num_vm_area ++; // incrementing
    new_vma->vm_next = NULL;
    unsigned long temporary = new_vma->vm_start;
    merge(prev, new_vma);
    return temporary;

}

/**
 * munmap system call implemenations
 */
long vm_area_unmap(struct exec_context *current, u64 addr, int length)
{
    
    if (length <= 0 || !current) {
        return -EINVAL;  // Invalid arguments
    }
    length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // Calculate the end address based on the given address and length
    u64 end_addr = addr + length ; // see this -1

    struct vm_area *prev = NULL;
    struct vm_area *vma = current->vm_area;
    if(!vma) return -EINVAL;  // no vm_area to unmap

    while (vma) {
        if (vma->vm_end <= addr) {
            // The current VMA is completely before the range to unmap
            prev = vma;
            vma = vma->vm_next;
            continue;
        }

        if (vma->vm_start >= end_addr) {
            // The current VMA is completely after the range to unmap
            break;
        }

        if (vma->vm_start < addr && vma->vm_end > end_addr) {
            // The current VMA spans across the entire range to unmap
            // We need to split this VMA into two VMAs
            get_pages_to_free(current, addr,end_addr);
            struct vm_area *new_vma = alloc_vm_area(end_addr, vma->vm_end - end_addr, vma->access_flags);
            if(!new_vma) return -EINVAL;
            stats->num_vm_area++;  // increment counter

            new_vma->vm_next = vma->vm_next;

            vma->vm_end = addr;
            vma->vm_next = new_vma;
            break;
        }

        if (vma->vm_start < addr) {
            // The VMA overlaps with the range at the start
            // We need to adjust the end of the VMA
            get_pages_to_free(current, addr,vma->vm_end );
            vma->vm_end = addr;
            
        } 
        
        else if (vma->vm_end > end_addr) {
            // The VMA overlaps with the range at the end
            // We need to adjust the start of the VMA
            get_pages_to_free(current, vma->vm_start, end_addr);
            vma->vm_start = end_addr;
            break;
        } else {
            // The VMA is completely within the range to unmap
            // We should remove this VMA
            prev->vm_next = vma->vm_next;
            get_pages_to_free(current, vma->vm_start, vma->vm_end);
            struct vm_area *temp = vma;
            vma = vma->vm_next;
            os_free(temp, sizeof(struct vm_area));
            stats->num_vm_area--;  // decrement counter
        }
    }

    return 0; // Success
}



/**
 * Function will invoked whenever there is page fault for an address in the vm area region
 * created using mmap
 */

long vm_area_pagefault(struct exec_context *current, u64 addr, int error_code)
{   

  
  
    if(!current) return -1;
    //if(addr < MMAP_AREA_START || addr > MMAP_AREA_END) return -1;
    struct  vm_area *vma = search(current, addr);
    if(!vma) {
        
        return -1; // no vma exists wala case

    }
    if(error_code == 0x6 && (vma->access_flags == PROT_READ)) return -1; // write demanded on read only page
    if(error_code == 0x7){
        if(vma->access_flags == PROT_READ) return -1; // execute demanded on read only page
        else if(vma->access_flags == (PROT_READ | PROT_WRITE)) {
            
            return handle_cow_fault(current, addr,  (PROT_READ | PROT_WRITE) );
            // see if more code req here
        } // execute demanded on write only page
    }

    if(error_code == 0x4 || error_code == 0x6 ){
    int i;
    u32 pfn;
    u64 *ptr;
   // u64* temp;
    
    pfn = current->pgd;
    for(i=0;i<4;i++){
        ptr = (u64*)osmap(pfn);
       
       // temp = ptr;
        ptr = ptr + ((addr >> (12 + 9*(3-i))) & 0x1FF);
       
       
        if(((*ptr)&1) == 0 ) {
         //   ptr = temp;
            break; // not mapped afterwards, deal with this

        }
        if((((*ptr)&0x8) == 0) && (vma->access_flags == PROT_READ | PROT_WRITE) ) { // write on vma but not on pte
            *ptr = (*ptr) | 0x8;
        }
        pfn = (*ptr) >> 12;

    }
    int permit_bit;
    
    if(vma->access_flags == PROT_READ | PROT_WRITE) permit_bit = 1;
    else permit_bit = 0;
    handle_faults(ptr,addr,i,permit_bit);
    
    }
    
    return 1;
}

/**
 * cfork system call implemenations
 * The parent returns the pid of child process. The return path of
 * the child process is handled separately through the calls at the 
 * end of this function (e.g., setup_child_context etc.)
 */

long do_cfork(){
    u32 pid;
    struct exec_context *new_ctx = get_new_ctx();
    struct exec_context *ctx = get_current_ctx();
     /* Do not modify above lines
     * 
     * */   
     /*--------------------- Your code [start]---------------*/
     if(!ctx || !new_ctx) return -1;

     pid = new_ctx->pid;
   
   
     int copied;
     memcpy(new_ctx,ctx, sizeof(struct exec_context));
     new_ctx->ppid = ctx->pid;
     new_ctx->pid = pid;

     u32 start_pfn = os_pfn_alloc(OS_PT_REG);
     new_ctx->pgd = start_pfn;
    
     initialize_page_table(osmap(start_pfn)); 
    
    
    get_pages_for_allocation(ctx, start_pfn);
    copy_linked_list(ctx, new_ctx);
   
    
    // check memcpy return value
     /*--------------------- Your code [end] ----------------*/
    
     /*
     * The remaining part must not be changed
     */
    copy_os_pts(ctx->pgd, new_ctx->pgd);
    do_file_fork(new_ctx);
    setup_child_context(new_ctx);
   
    // printk("parent files = %x, child files = %x", ctx->files[3], new_ctx->files[3]);
    return pid;
}



/* Cow fault handling, for the entire user address space
 * For address belonging to memory segments (i.e., stack, data) 
 * it is called when there is a CoW violation in these areas. 
 *
 * For vm areas, your fault handler 'vm_area_pagefault'
 * should invoke this function
 * */

long handle_cow_fault(struct exec_context *current, u64 vaddr, int access_flags)
{ 
  
  if(!current) return -1;

  
  int permit_bit;
  
  if(access_flags & PROT_WRITE != 0) permit_bit = 1;
  else permit_bit = 0;

  if(permit_bit == 0) return -1; // it is not a case of cow
  int ret = page_table_walk_cow(current, vaddr, permit_bit);

  flush_tlb();
  return ret;
}
