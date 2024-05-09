#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
#include<tracer.h>


///////////////////////////////////////////////////////////////////////////
//// 		Start of Trace buffer functionality 		      /////
///////////////////////////////////////////////////////////////////////////
int arguments(u64 n)
{
	// implement switch case if syscall_no is not valid return -1;
	if (n == 1)
		return 1;
	else if (n == 2)
		return 0;
	else if (n == 4)
		return 2;
	else if (n == 5)
		return 3;
	else if (n == 6)
		return 1;
	else if (n == 7)
		return 1;
	else if (n == 8)
		return 2;
	else if (n == 9)
		return 2;
	else if (n == 10)
		return 0;
	else if (n == 11)
		return 0;
	else if (n == 12)
		return 1;
	else if (n == 13)
		return 0;
	else if (n == 14)
		return 1;
	else if (n == 15)
		return 0;
	else if (n == 16)
		return 4;
	else if (n == 17)
		return 2;
	else if (n == 18)
		return 3;
	else if (n == 19)
		return 1;
	else if (n == 20)
		return 0;
	else if (n == 21)
		return 0;
	else if (n == 22)
		return 0;
	else if (n == 23)
		return 2;
	else if (n == 24)
		return 3;
	else if (n == 25)
		return 3;
	else if (n == 27)
		return 1;
	else if (n == 28)
		return 2;
	else if (n == 29)
		return 1;
	else if (n == 30)
		return 3;
	else if (n == 35)
		return 4;
	else if (n == 36)
		return 1;
	else if (n == 37)
		return 2;
	else if (n == 38)
		return 0;
	else if (n == 39)
		return 3;
	else if (n == 40)
		return 2;
	else if (n == 41)
		return 3;
	else if (n == 61){
		return 0;
	}		
	else
		return -1;
}

int is_valid_mem_range(unsigned long buff, u32 count, int access_bit) 
{

	struct exec_context *current = get_current_ctx();   // R = 1, W = 2
	struct vm_area *vm_area = current->vm_area;

	//printk("%x,%x" , current->mms[MM_SEG_CODE].start, current->mms[MM_SEG_CODE].next_free);
	if(current->mms[MM_SEG_CODE].start <= buff && current->mms[MM_SEG_CODE].next_free >= buff + count ){
		if((access_bit/2)%2){
				; // careful
			}
			else{
				return 1;
			}
	}
	
	if(current->mms[MM_SEG_RODATA].start <= buff && current->mms[MM_SEG_RODATA].next_free  >= buff + count ){
		if((access_bit/2)%2){
				;   
			}
			else{
				return 1;   
			}
	}

	if(current->mms[MM_SEG_DATA].start <= buff && current->mms[MM_SEG_DATA].next_free   >= buff + count ){
		// check for execute permission ? 
				return 1;
			
	}
	

	if(current->mms[MM_SEG_STACK].start <= buff && current->mms[MM_SEG_STACK].end  >= buff + count ){
		// check for execute permission ? 
				return 1;
			
	}
	
	struct vm_area *starting = vm_area;
	if(!starting) return 0; // nothing in vm area
	if(vm_area->vm_start <= buff && vm_area->vm_end  >= buff + count ){
			if(((access_bit)%2 && !((vm_area->access_flags%2))) || ((access_bit/2)%2 && !((vm_area->access_flags/2)%2)) || ((access_bit/4)%2 && !((vm_area->access_flags/4)%2)) ){
				;
			}
			else{
				return 1;
			}
		}
	
	vm_area = vm_area->vm_next;
	while(vm_area != NULL && vm_area != starting){   // circular linked list
		
		if(vm_area->vm_start <= buff && vm_area->vm_end  >= buff + count ){
			//printk("addr is : %x %x\n", vm_area->vm_start, vm_area->vm_end);
			if(((access_bit)%2 && !((vm_area->access_flags%2))) || ((access_bit/2)%2 && !((vm_area->access_flags/2)%2)) || ((access_bit/4)%2 && !((vm_area->access_flags/4)%2)) ){
				;
			}
			else{
				
				return 1;
			}
		}
		vm_area = vm_area->vm_next;
	}
	
	
	return 0;
}



long trace_buffer_close(struct file *filep)
{	
	if(filep == NULL || filep->type != TRACE_BUFFER || filep->trace_buffer == NULL || filep->fops == NULL || filep->trace_buffer->buff == NULL){
		return -EINVAL;
	}
	os_page_free(USER_REG, filep->trace_buffer->buff);    // should we check for null ? 
	//filep->trace_buffer->buff = NULL;
	os_free(filep->trace_buffer, sizeof(struct trace_buffer_info));
	//filep->trace_buffer = NULL;
	//os_page_free(USER_REG, filep->trace_buffer);
	os_free(filep->fops, sizeof(struct fileops));
	//filep->fops = NULL;
	//os_page_free(USER_REG, filep->fops);
	//struct exec_context* current = get_current_ctx();
	// for(int i = 0; i  < MAX_OPEN_FILES; i++){
	// 	if(current->files[i] == filep){
	// 		current->files[i] = NULL;
	// 		break;
	// 	}
	// }
	os_free(filep, sizeof(struct file));
	return 0;	
}



int trace_buffer_read(struct file *filep, char *buff, u32 count)
{	// more error handling needed
	if(count<0) return -EINVAL;
	if(buff == NULL || !is_valid_mem_range((unsigned long) buff, count, 2  )){
		return -EBADMEM;
	}
	if(filep == NULL || filep->type != TRACE_BUFFER || filep->mode == O_WRITE || count < 0){
		return -EINVAL;
	}
	struct trace_buffer_info* trace_buffer = filep->trace_buffer;
	if(trace_buffer == NULL){
		return -EINVAL;
	}

	u32 read_off = trace_buffer->read_off;
	if(read_off < 0 || read_off >= TRACE_BUFFER_MAX_SIZE){
		return -EINVAL;
	}
	
	u32 no_read = 0;

	while(no_read < count && trace_buffer->size > 0){
		
		buff[no_read] = trace_buffer->buff[trace_buffer->read_off];
		//trace_buffer->buff[trace_buffer->read_off] = 0;
		trace_buffer->read_off = (trace_buffer->read_off + 1)%TRACE_BUFFER_MAX_SIZE;
		no_read++;
		trace_buffer->size--;
		
	}
	
	return no_read;
}

int trace_buffer_write(struct file *filep, char *buff, u32 count)
{		// more error handling needed 
	if(count < 0) return -EINVAL;
	if(buff == NULL || !is_valid_mem_range((unsigned long) buff, count, 1  )){
		
		return -EBADMEM;
	}
	if(filep == NULL || filep->type != TRACE_BUFFER || filep->mode == O_READ || count < 0){  // count negative ka case
		return -EINVAL;
	}
	struct trace_buffer_info* trace_buffer = filep->trace_buffer;
	if(trace_buffer == NULL){
		
		return -EINVAL;
	}
	
	u32 no_write = 0;


	while(no_write < count && trace_buffer->size < TRACE_BUFFER_MAX_SIZE ){
		if(buff+no_write == NULL){
			return -EINVAL;
		}
		trace_buffer->buff[trace_buffer->write_off] = buff[no_write];
		trace_buffer->write_off = (trace_buffer->write_off + 1)%TRACE_BUFFER_MAX_SIZE;
		no_write++;
		trace_buffer->size++;
	}
	
	return no_write;
}
int write_from_os_mode(struct file *filep, char *buff, u32 count){
	
	if(buff == NULL){
		return -EINVAL;
	}
	if(filep == NULL || filep->type != TRACE_BUFFER || filep->mode == O_READ || count < 0){  // count negative ka case
		return -EINVAL;
	}
	struct trace_buffer_info* trace_buffer = filep->trace_buffer;
	if(trace_buffer == NULL){
		return -EINVAL;
	}
	u32 no_write = 0;
	while(no_write < count && trace_buffer->size < TRACE_BUFFER_MAX_SIZE ){
		if(buff+no_write == NULL){
			return -EINVAL;
		}
		trace_buffer->buff[trace_buffer->write_off] = buff[no_write];
		trace_buffer->write_off = (trace_buffer->write_off + 1)%TRACE_BUFFER_MAX_SIZE;
		no_write++;
		
		trace_buffer->size++;
	}
	
	return no_write;

}

int sys_create_trace_buffer(struct exec_context *current, int mode)
{
	int lowest_descriptor;
	
	int i = 0;
	if(!current || (mode != O_RDWR && mode != O_READ && mode != O_WRITE)){
		return -EINVAL;
	}
	while(i< MAX_OPEN_FILES && current->files[i]!= NULL){
		i++;
	}
	if(i == MAX_OPEN_FILES){
		return -EINVAL;
	}
	lowest_descriptor = i;
	
	struct file* filep = (struct file*)os_alloc(sizeof(struct file));

	if(filep == NULL){
		return -ENOMEM;
	}
	filep->inode = NULL;
	filep->type = TRACE_BUFFER;
	filep->mode = mode;
	filep->offp = 0;
	filep->ref_count = 1; // iska dhyan rakhna
	struct trace_buffer_info* trace_buffer = (struct trace_buffer_info*)os_alloc(sizeof(struct trace_buffer_info));
	//struct trace_buffer_info* trace_buffer = (struct trace_buffer_info*)os_page_alloc(USER_REG);
	if(filep == NULL){
		return -ENOMEM;
	}
	trace_buffer->read_off = 0;
	trace_buffer->write_off = 0;
	trace_buffer->size = 0;
	trace_buffer->buff = (char*)os_page_alloc(USER_REG);
	if(trace_buffer->buff == NULL){
		
		return -ENOMEM;
	}
	// for(int j=0; j<TRACE_BUFFER_MAX_SIZE; j++){
	// 	trace_buffer->buff[j] = 0;
	// }
	filep->trace_buffer = trace_buffer; // trace buffer is a pointer to trace_buffer_info
	struct fileops* fileops = (struct fileops*)os_alloc(sizeof(struct fileops));
	//struct fileops* fileops = (struct fileops*)os_page_alloc(USER_REG);
	if(fileops == NULL){
		return -ENOMEM;
	}

	fileops->read = trace_buffer_read;
	fileops->write = trace_buffer_write;
	fileops->close = trace_buffer_close;
	filep->fops = fileops;  // fileops is a pointer to fileops
	current->files[lowest_descriptor] = filep; // filep is a pointer to file
	

	return lowest_descriptor;
	
	// in case of success return the file descriptor

}

///////////////////////////////////////////////////////////////////////////
//// 		Start of strace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////


int perform_tracing(u64 syscall_num, u64 param1, u64 param2, u64 param3, u64 param4)
{
	
	if(syscall_num == 1 || syscall_num == 37 || syscall_num == 38) return 0; // exit, end_strace and start_strace will not be handled
	struct exec_context* current = get_current_ctx(); 
	if(!current) return -EINVAL;
	if(current->st_md_base != NULL  && current->st_md_base->is_traced){
		//u64 *array = (u64*)os_alloc(5*sizeof(u64));  // avoided os alloc here
		u64 array[5];
		//if(array == NULL) return -EINVAL;
		u64 params = arguments(syscall_num);
		array[0] = syscall_num;
		array[1] = param1;
		array[2] = param2;
		array[3] = param3;
		array[4] = param4;

		if(params == -1) return -EINVAL;
		
		
		int write;
		
		if(current->st_md_base->tracing_mode == FULL_TRACING){
			
			write = write_from_os_mode(current->files[current->st_md_base->strace_fd], (char*)array, 8);
			if(write != 8) return -EINVAL;
			for(int i = 1; i<=params; i ++){
				write = write_from_os_mode(current->files[current->st_md_base->strace_fd], (char*)(array + i), 8);
				if(write != 8) return -EINVAL;
			}
			//printk("pehle aaya\n");
			//os_free(array, 5*sizeof(u64));
			//printk(" is it freeing %d\n", array[1]);
			//os_free(array, 5*sizeof(u64));
			return 0;
		}
		else if(current->st_md_base->tracing_mode == FILTERED_TRACING){
			struct strace_info* curr = current->st_md_base->next;
			while(curr != NULL){
				//printk("curr->syscall_num : %d, syscall_num : %d\n", curr->syscall_num, syscall_num);
				if(curr->syscall_num == syscall_num){
					
					write = write_from_os_mode(current->files[current->st_md_base->strace_fd], (char*)array, 8);
					if(write != 8) return -EINVAL;
					for(int i = 1; i<=params; i ++){
						write = write_from_os_mode(current->files[current->st_md_base->strace_fd], (char*)(array + i), 8);
						if(write != 8) return -EINVAL;
					}
					return 0;
					
				}
				curr = curr->next;

			}
			//printk("pehle aaya\n");
			//os_free(array, 5*sizeof(u64));
			//printk("is it freeing %d\n", array[1]);
			//os_free(array, 5*sizeof(u64));
		
			return 0;
		}
		else return -EINVAL;
	}
	
	return 0;
}


int sys_strace(struct exec_context *current, int syscall_num, int action)
{	
	if(!current) return -EINVAL; 
	if(syscall_num < 0 || (action != ADD_STRACE && action != REMOVE_STRACE)) return -EINVAL;
	if(arguments(syscall_num) == -1) return -EINVAL; // invalid syscall
	
	 
	if(action == ADD_STRACE){
		if(!current->st_md_base) {
		current->st_md_base = (struct strace_head*)os_alloc(sizeof(struct strace_head));
		//current->st_md_base = (struct strace_head*)os_page_alloc(USER_REG);
		if(current->st_md_base == NULL) return -EINVAL;
		current->st_md_base->is_traced = 0;
		current->st_md_base->tracing_mode = 0;
		current->st_md_base->count = 0;
		current->st_md_base->strace_fd = -1;
		current->st_md_base->next = NULL;
		current->st_md_base->last = NULL;
	}   
		if(current->st_md_base->count == STRACE_MAX) { // max limit reached
			return -EINVAL; // see if return error here
		}
		struct strace_info* curr = current->st_md_base->next;
		while(curr != NULL){
			if(curr->syscall_num == syscall_num)
				return -EINVAL;   // see if this should be an error			}
			curr = curr->next;
		}
		if(current->st_md_base->next == NULL){
			current->st_md_base->next = (struct strace_info*)os_alloc(sizeof(struct strace_info));
			//current->st_md_base->next = (struct strace_info*)os_page_alloc(USER_REG);
			if(current->st_md_base->next == NULL) return -EINVAL;
			current->st_md_base->next->syscall_num = syscall_num;
			current->st_md_base->next->next = NULL;
			current->st_md_base->last = current->st_md_base->next;
			//printk("inside null one,syscall num : %d", current->st_md_base->last->syscall_num);
		}
		else {
			struct strace_info* temp = (struct strace_info*)os_alloc(sizeof(struct strace_info));
			//struct strace_info* temp = (struct strace_info*)os_page_alloc(USER_REG);
			if(temp == NULL) return -EINVAL;
			temp->syscall_num = syscall_num;
			temp->next = NULL;
			current->st_md_base->last->next = temp;
			current->st_md_base->last = temp;
			//printk("inside non null ,syscall num is : %d", current->st_md_base->last->syscall_num);
		}
		current->st_md_base->count ++;
		//printk("count is: %d, syscall num is %d, at end is : %d \n", current->st_md_base->count,syscall_num, current->st_md_base->last->syscall_num);
		return 0;
		
	}
	else {
		if(!current->st_md_base) return -EINVAL;
		struct strace_info* curr = current->st_md_base->next;
		struct strace_info* prev = NULL;
		while(curr){
			if(curr->syscall_num == syscall_num){
				
				if(prev){
					prev->next = curr->next;
				}
				else{
					current->st_md_base->next = curr->next;
				}
				if(curr == current->st_md_base->last){
					current->st_md_base->last = prev;
				}
				current->st_md_base->count --;
				os_free(curr, sizeof(struct strace_info));
				return 0;
			}
			prev = curr;
			curr = curr->next;
		}
		return -EINVAL; // not found in the list at all, so removing makes no sense
	}

	//return 0;
}

int sys_read_strace(struct file *filep, char *buff, u64 count)
{	
	if(buff == NULL || filep == NULL || filep->type != TRACE_BUFFER || filep->trace_buffer == NULL || count < 0) {
		
		return -EINVAL;
	}
	int i, read, j;
	int bytes = 0;
	
	u64 num, params;
	for(i = 0;i < count; i ++){
		read = trace_buffer_read(filep,buff,8);
		
		//if(read != 8) return -EINVAL;
		if(read == 0) break;    // no more info in trace buffer to use, return the current value of bytes read
		if(read == -EINVAL || read == -EBADMEM) return -EINVAL;
		
		bytes += 8;
		num = *((u64*)buff);
		params = arguments(num);
		if(params == -1) return -EINVAL; // invalid syscall
		buff = buff + 8;
		for(j=0;j<params;j++){
			read = trace_buffer_read(filep,buff,8);
			if(read != 8) {
				
				return -EINVAL;   // if parameter there then 8 should have been read
			}
			bytes += 8;
			buff = buff + 8;
			
		}
	}
	
	
	return bytes;
}

int sys_start_strace(struct exec_context *current, int fd, int tracing_mode)
{	
	if(!current || (tracing_mode != FULL_TRACING && tracing_mode != FILTERED_TRACING)) return -EINVAL;
	if(fd < 0 || fd >= MAX_OPEN_FILES) return -EINVAL;

	//if(current->st_md_base && current->st_md_base->is_traced) return -EINVAL;
	if(current->files[fd] == NULL || current->files[fd]->type != TRACE_BUFFER) return -EINVAL;
	if(!current->st_md_base) {
		current->st_md_base = (struct strace_head*)os_alloc(sizeof(struct strace_head));
		//current->st_md_base = (struct strace_head*)os_page_alloc(USER_REG);
		if(current->st_md_base == NULL) return -EINVAL;
		current->st_md_base->next = NULL;
		current->st_md_base->last = NULL;
		current->st_md_base->count = 0;
	}
	current->st_md_base->is_traced = 1;
	current->st_md_base->tracing_mode = tracing_mode;
	
	current->st_md_base->strace_fd= fd;

	return 0;
}

int sys_end_strace(struct exec_context *current)
{
	if(!current || !(current->st_md_base) || !current->st_md_base->is_traced) return -EINVAL;
	struct strace_info* curr = current->st_md_base->next;
	struct strace_info* temp;
	while(curr != NULL){
		temp = curr;
		curr = curr->next;
		os_free(temp, sizeof(struct strace_info));
		//os_page_free(USER_REG, temp);

	}
	current->st_md_base->next = NULL;
	current->st_md_base->last = NULL;
	// current->st_md_base->is_traced = 0;
	// current->st_md_base->count = 0;

	os_free(current->st_md_base, sizeof(struct strace_head));
	//os_page_free(USER_REG, current->st_md_base);
	current->st_md_base = NULL;
	return 0;

}



///////////////////////////////////////////////////////////////////////////
//// 		Start of ftrace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////


long do_ftrace(struct exec_context *ctx, unsigned long faddr, long action, long nargs, int fd_trace_buffer)
{
    if(ctx == NULL) return -EINVAL;
	if(ctx->ft_md_base == NULL){
		ctx->ft_md_base = (struct ftrace_head*)os_alloc(sizeof(struct ftrace_head));
		//ctx->ft_md_base = (struct ftrace_head*)os_page_alloc(USER_REG);
		if(ctx->ft_md_base == NULL) return -EINVAL;
		ctx->ft_md_base->next = NULL;
		ctx->ft_md_base->last = NULL;
		ctx->ft_md_base->count = 0;
	}
	if(action == ADD_FTRACE){
		struct ftrace_info* curr = ctx->ft_md_base->next;
		while(curr != NULL){
			if(curr->faddr == faddr){
				return -EINVAL;
			}
			curr = curr->next;
		}
		if(ctx->ft_md_base->count == MAX_FTRACE) return -EINVAL;

		if(ctx->ft_md_base->next == NULL){
			ctx->ft_md_base->next = (struct ftrace_info*)os_alloc(sizeof(struct ftrace_info));
			//ctx->ft_md_base->next = (struct ftrace_info*)os_page_alloc(USER_REG);
			if(ctx->ft_md_base->next == NULL) return -EINVAL;
			ctx->ft_md_base->next->faddr = faddr;
			ctx->ft_md_base->next->num_args = nargs;
			ctx->ft_md_base->next->next = NULL;
			ctx->ft_md_base->next->fd = fd_trace_buffer;
			ctx->ft_md_base->next->capture_backtrace = 0;
			ctx->ft_md_base->last = ctx->ft_md_base->next;
		}
		else {
			struct ftrace_info* temp = (struct ftrace_info*)os_alloc(sizeof(struct ftrace_info));
			//struct ftrace_info* temp = (struct ftrace_info*)os_page_alloc(USER_REG);
			if(temp == NULL) return -EINVAL;
			temp->faddr = faddr;
			temp->num_args = nargs;
			temp->next = NULL;
			temp->fd = fd_trace_buffer;
			temp->capture_backtrace = 0;
			ctx->ft_md_base->last->next = temp;
			ctx->ft_md_base->last = temp;
		}
		ctx->ft_md_base->count ++;
		return 0;
	}
	else if(action == REMOVE_FTRACE){

		if(ctx->ft_md_base == NULL) return -EINVAL;
		struct ftrace_info* curr = ctx->ft_md_base->next;
		struct ftrace_info* prev = NULL;
		while(curr){
			if(curr->faddr == faddr){
				u8* memory = (u8*)faddr;
				if(*memory == INV_OPCODE && *(memory + 1) == INV_OPCODE && *(memory + 2) == INV_OPCODE && *(memory + 3) == INV_OPCODE){
					for(int i=0;i<4;i++){
					
					*((u8*)(faddr+i)) = curr->code_backup[i];  // if tracing is enables for this function, need to disable tracing before removing it from the list 
				}
				}

				
				if(prev){
					prev->next = curr->next;
				}
				else{
					ctx->ft_md_base->next = curr->next;
				}
				if(curr == ctx->ft_md_base->last){
					ctx->ft_md_base->last = prev;
				}
				ctx->ft_md_base->count --;
				os_free(curr, sizeof(struct ftrace_info));
				return 0;
			}
			prev = curr;
			curr = curr->next;
		}
		
		return -EINVAL; // not found in the list at all, so removing makes no sense


	}
	else if(action == ENABLE_FTRACE){
		int i, flag = 0;
		u8* to_modify;
		if(ctx->ft_md_base == NULL) return -EINVAL;
		struct ftrace_info* curr = ctx->ft_md_base->next;
		while(curr){
			if(faddr == curr->faddr){
				flag = 1;
				break;
			}
			curr = curr->next;
		}
		if(flag == 0) return -EINVAL;   // the one to be enables must be present in the list beforehand
		to_modify = (u8*) faddr;
		
		if(*to_modify != INV_OPCODE || *(to_modify + 1) != INV_OPCODE || *(to_modify + 2) != INV_OPCODE || *(to_modify + 3) != INV_OPCODE){

					for(int i=0;i<4;i++){
					curr->code_backup[i] = *(to_modify+i);
					*(to_modify+i) = INV_OPCODE;
					}
				}
		return 0;
		
		
	}
	else if(action == DISABLE_FTRACE){
		int i;
		u8* to_modify;
		if(ctx->ft_md_base == NULL) return -EINVAL;
		struct ftrace_info* curr = ctx->ft_md_base->next;
		while(curr){
			if(faddr == curr->faddr){
				to_modify = (u8*) faddr;
				if(*to_modify == INV_OPCODE && *(to_modify + 1) == INV_OPCODE && *(to_modify + 2) == INV_OPCODE && *(to_modify + 3) == INV_OPCODE){ // if it was enabled, then disable
				for(i=0;i<4;i++){
					
					*(to_modify+i) = curr->code_backup[i];
				}
				}
				
				return 0;
			}
			curr = curr->next;
		}
		return -EINVAL;   // the one to be enables must be present in the list beforehand
		
	}
	else if(action == ENABLE_BACKTRACE){
		if(ctx->ft_md_base == NULL) return -EINVAL;
		struct ftrace_info* curr = ctx->ft_md_base->next;
		while(curr){
			if(faddr == curr->faddr){
				curr->capture_backtrace = 1;
				u8* memory = (u8*)faddr;
				if(*memory != INV_OPCODE || *(memory + 1) != INV_OPCODE || *(memory + 2) != INV_OPCODE || *(memory + 3) != INV_OPCODE){

					for(int i=0;i<4;i++){
					curr->code_backup[i] = *(memory+i);
					*(memory+i) = INV_OPCODE;
					}
				}
				return 0;
			}
			curr = curr->next;
		}
		return -EINVAL; 
	}
	else if(action == DISABLE_BACKTRACE){
		if(ctx->ft_md_base == NULL) return -EINVAL;
		struct ftrace_info* curr = ctx->ft_md_base->next;
		u8* to_modify;

		while(curr){
			if(faddr == curr->faddr){
				if(curr->capture_backtrace == 0) return 0; // already disabled (not enabled) , see if this have to be an error
				to_modify = (u8*)faddr;
				for(int i=0;i<4;i++){
					*(to_modify+i) = curr->code_backup[i];  // disable normal tracing as well
				}
				curr->capture_backtrace = 0;
				return 0;
			}
			curr = curr->next;
		}
		return -EINVAL;  // the one to be disabled must be present in the list beforehand
	}
	return -EINVAL;

	
}

//Fault handler
long handle_ftrace_fault(struct user_regs *regs)
{
 
	
		u64 faddr = regs->entry_rip;
		u64 params[6];
		params[0] = regs->rdi;
		params[1] = regs->rsi;
		params[2] = regs->rdx;
		params[3] = regs->rcx;
		params[4] = regs->r8;
		params[5] = regs->r9;
		struct exec_context *current = get_current_ctx();
		if(current == NULL) return -EINVAL;
		if(current->ft_md_base == NULL) return -EINVAL;
		struct ftrace_info* curr = current->ft_md_base->next;
		int flag = 0;
		u64 inside = 0;
		while(curr){
			if(faddr == curr->faddr){
				flag = 1;
				break;
			}
			curr = curr->next;
		}
		if(flag == 0) return -EINVAL;
		int write;
		
		
		int fd = curr->fd;
		if(fd < 0 || fd >= MAX_OPEN_FILES) return -EINVAL;
		struct file* filep = current->files[fd];
		if(filep == NULL || filep->type != TRACE_BUFFER) return -EINVAL;
		
		u64 temp_store[512];
		u64* starting = temp_store;
		int j = 1;
		//write = write_from_os_mode(filep, (char*)&faddr, 8);
		//if(write != 8) return -EINVAL;
		temp_store[j] = faddr;
		j++;
		for(int i = 0; i<curr->num_args; i ++){
			temp_store[j] = params[i];
			j++;
		}
		inside += 1;
		inside += curr->num_args;
		
		regs->entry_rsp -= 8;
		*((u64*)(regs->entry_rsp)) = regs->rbp;
		regs->rbp = regs->entry_rsp;
		regs->entry_rip = faddr + 4;

		if(curr->capture_backtrace == 1){
			//write = write_from_os_mode(filep, (char*)&faddr, 8); // address of first intr of function
			temp_store[j] = faddr;
			j++;
			inside += 1;
			//if(write != 8) return -EINVAL;
			u64 back_flag;
			u64 sp,bp;
			back_flag = *((u64*)(regs->rbp+8) );
			bp = *((u64*)regs->rbp);
			while(back_flag != END_ADDR){
				//write = write_from_os_mode(current->files[curr->fd], (char*)&back_flag, 8);
				//if(write != 8) return -EINVAL;
				temp_store[j] = back_flag;
				j++;
				inside += 1;
				back_flag = *((u64*)(bp + 8));
				bp = *((u64*)bp);

				
			}
			// backtracing code here 
		}
		//u64 var = END_ADDR;

		//write = write_from_os_mode(filep, (char*)&var, 8);
		//if(write != 8) return -EINVAL;
		temp_store[0] = inside;
		write = write_from_os_mode(filep, (char*)temp_store, 8*(inside+1));
		if(write != 8*(inside+1)) return -EINVAL;
		
        return 0;
}


int sys_read_ftrace(struct file *filep, char *buff, u64 count)
{	
	
    if(buff == NULL || filep == NULL || filep->type != TRACE_BUFFER || filep->trace_buffer == NULL || count < 0 ) {
		
		return -EINVAL;
	}
	int i, read, j;
	int bytes = 0;
	
	u64 num, params;
	u64 inside;
	for(i = 0;i < count; i ++){ 
	
		read = trace_buffer_read(filep,buff,8);
		
		if(read == 0) break;    // no more info in trace buffer to use, return the current value of bytes read
		if(read == -EINVAL || read == -EBADMEM) return -EINVAL;
		inside = *((u64*)buff);
		for(j = 0;j<inside; j++){
			read = trace_buffer_read(filep,buff,8);
			if(read != 8) return -EINVAL;
			bytes += 8;
			buff = buff + 8;
		}
		//bytes += 8;
		// while(*(u64*)buff != END_ADDR){ // see if this is correct
		// 	buff = buff + 8;
		// 	read = trace_buffer_read(filep,buff,8);
		// 	if(read != 8) {
				
		// 		return -EINVAL;   // if parameter there then 8 should have been read
		// 	}
		// 	if(*(u64*)buff != END_ADDR) bytes += 8;
			
			
		// }
		
	}

	
	return bytes;

}


