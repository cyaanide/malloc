#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

#define DEBUG

size_t* S_NULL = (size_t*) 0;
size_t* heap_start;

/*
void heap_checker(void){
	size_t* header = heap_start;
	size_t prev_alloc = 2;
	size_t* end = (size_t*) (((char*) mem_heap_hi()) + 1);
	while(header < end){
		//check the allocation according to prev allocation

		if((GET_ALLOC_USING_BP(bp) * 2) != prev_alloc){
			printf("allocation variable not according to prev all")

		}


	}
}
*/

/*
 * get_class - given size, find the class it belongs to.
 * contract-
 * 1. size is no of blocks
 */
static size_t get_class(size_t size){

	// convert size from blocks to bytes, as thats what is stored physically
	size = size * (BLOCK_SIZE);
	size_t bit_position = 0;

	// explicit use of size_t one as to stop compiler from using an int '1' which does not gurantee all 0's
	// after the 32nd bit due to how its represented internally
	size_t probe = 1;
	for(size_t i = 0; i < len_segment_list; ++i){
		if(size & probe){
			bit_position = i;
		}
		probe = probe << 1;
	}
	return bit_position;
}

/*
 * create_free_block - given the base pointer, create a free block with size no of blocks
 * contract-
 * 1.size is no of blocks
 * 2.Does not change the previous allocation variable given by the header
 * 3.changes footer and header to be free and have size = size
 */
static void create_free_segment(size_t** bp, size_t size){

	// convert size to bytes
	size = size * BLOCK_SIZE;

	// size in byes should be a multiple of 8
	if(size%8 != 0){
		printf("create_free_segment: size in bytes is not multiple of 8. size in bytes = %zu\n", size);
		exit(0);
	}

	size_t* header = GET_HDR_USING_BP(bp);

	// Remember the allocation variable 
	size_t header_mask = *header & (0x2);

	// Put in the size and restore the allocation variable
	// NOTE that what is in header isnt no of blocks but no of bytes
	*header = size;
	*header = *header | header_mask;

	// Put in size at footer
	// The macro only works when the block has a header with well defined size
	size_t* footer = GET_FTR_USING_BP(bp);
	*footer = size;

	// Update the allocation variable for the next segment
	size_t* nxt_header = GET_NXT_HDR_USING_BP(bp);
	*nxt_header = *nxt_header & ~(0x2);
}

/*
 * coalesce - coalesce neighbournig blocks
 * returns the new bp
 */
size_t** coalesce(size_t** bp){	

	size_t size = GET_SIZE_USING_BP(bp);
	size_t nxt_alloc = GET_NEXT_ALLOC_USING_BP(bp);
	size_t prv_alloc = GET_PREV_ALLOC_USING_BP(bp);

	// both next and prv are allocated
	if(nxt_alloc && prv_alloc){
		return bp;
	}
	// next is free and prev is allocated
	else if(!nxt_alloc && prv_alloc){
		printf("case 2\n");
		print_free_list();
		size_t** nxt_bp = GET_NXT_BP_USING_BP(bp);
		size_t validity = validate_free_segment(nxt_bp);
		printf("validity of nxt is %zu\n", validate_free_segment(nxt_bp));
		size_t sz_nxt = GET_SIZE_USING_BP(nxt_bp);
		size_t total_size = size + sz_nxt + BLOCKS_IN_ONE_HEADER;

		// remove the next block from the segment list
		remove_from_segment_list(nxt_bp);

		create_free_segment(bp, total_size);
		printf("validity of result is %zu\n", validate_free_segment(bp));
		return bp;
	}
	// prev is free and next is allocated
	else if(nxt_alloc && !prv_alloc){
		printf("case 3\n");
		size_t** prv_bp = GET_BP_USING_FTR(GET_PREV_FTR_USING_BP(bp));
		printf("validity of prv is %zu\n", validate_free_segment(prv_bp));
		size_t sz_prv = GET_SIZE_USING_BP(prv_bp);
		size_t total_size = size + sz_prv + BLOCKS_IN_ONE_HEADER;

		remove_from_segment_list(prv_bp);

		create_free_segment(prv_bp, total_size);
		printf("validity of result is %zu\n", validate_free_segment(prv_bp));
		return prv_bp; 
	}
	// both prev and next are allocated
	else{
		printf("case 4\n");
		size_t** prv_bp = GET_BP_USING_FTR(GET_PREV_FTR_USING_BP(bp));
		printf("validity of prv is %zu\n", validate_free_segment(prv_bp));
		size_t sz_prv = GET_SIZE_USING_BP(prv_bp);

		size_t** nxt_bp = GET_NXT_BP_USING_BP(bp);
		printf("validity of nxt is %zu\n", validate_free_segment(nxt_bp));
		size_t sz_nxt = GET_SIZE_USING_BP(nxt_bp);

		size_t total_size = sz_prv + sz_nxt + (2 * BLOCKS_IN_ONE_HEADER);

		remove_from_segment_list(prv_bp);
		remove_from_segment_list(nxt_bp);

		create_free_segment(prv_bp, total_size);
		printf("validity of result is %zu\n", validate_free_segment(prv_bp));

		return prv_bp;
	}
}


/*
 * split_free_block - split a bigger block into two smaller ones
 * slits the block if another block with payload of 'size' can be created
 * returns the bp of the extra block if split was successful or else returns S_NULL
 */
static size_t** split_free_segment(size_t** bp, size_t size){
	
size_t size_original = GET_SIZE_USING_BP(bp);
#ifdef DEBUG
	size_t validation = validate_free_segment(bp);
	if(!validation){
		printf("split_free_block: trying to split an invalid block\n");
		exit(0);
	}
	size_t allocation = GET_ALLOC_USING_BP(bp);
	if(allocation){
		printf("split_free_block: trying to split an allocated block\n");
		exit(0);
	}
	if(size < MIN_BLOCKS_IN_FREE){
		printf("split_free_block: size less than min no of blocks in free\n");
		exit(0);
	}
	if((size * BLOCK_SIZE)%8 != 0){
		printf("split_free_block: split_free_block: split size is not aligned\n");
		exit(0);
	}
	if(size > size_original){;
		printf("split_free_block: size > size_original");
	}
#endif


	// if the size is equal to the original (it should not be greater than original under no circumstances)
	// if size is = or > then original size then remaining size will have integer underflow and it will create
	// a very bad split
	if(size == size_original){
		return (size_t**) S_NULL;
	}
	size_t remaining_size = size_original;
	// remove space for the main block
	remaining_size -= size;
	// remove space for a header
	remaining_size -= BLOCKS_IN_ONE_HEADER;
	// remaining_size is now the no of blocks left for the new free segment if we decide to split

	// can split
	if(remaining_size >= MIN_BLOCKS_IN_FREE){

		// create the main block
		// create_free_block does not change the allocation variable which is desirable
		create_free_segment(bp, size);

		// create the extra block
		size_t* next_hdr = GET_NXT_HDR_USING_BP(bp);
		size_t** next_bp = GET_BP_USING_HDR(next_hdr);
		// create_free_block does not change the allocation variable which in this case is
		// not desirable, manually change the allocation variable to zero
		create_free_segment(next_bp, remaining_size);
		*next_hdr = *next_hdr & ~(0x2);

		return next_bp;
	}

	// could not split
	return (size_t**) S_NULL;
}

/* 
 * allocate_segment - allocates a segment
 */
void allocate_segment(size_t** bp){

	#ifdef DEBUG
		if(GET_ALLOC_USING_BP(bp)){
			printf("allocate_segment: trying to allocate already allocated block\n");
			exit(0);
		}
		size_t** temp;
		for(size_t i = 0; i < len_segment_list; i++){
			temp = (size_t**) metadata[i];
			while((size_t*) temp != S_NULL){
				if(temp == bp){
					printf("trying to allocate a block that is in a free list\n");
					exit(0);
				}
				temp = (size_t**) temp[NEXT];
			}
		}
	#endif

	size_t* header = GET_HDR_USING_BP(bp);
	*header = *header | 0x1;

	// Also update the allocation variable of the next segment
	size_t* nxt_hdr = GET_NXT_HDR_USING_BP(bp);
	*nxt_hdr = *nxt_hdr | 0x2;
}


/*
 * find_free_segment- search the free list for a free segment
 * size is no of blocks
 */
static size_t** find_free_segment(size_t size){

#ifdef DEBUG
	if(size < 4){
		printf("find_free_segment: trying to find a block with size < 4\n");
		exit(0);
	}
	if((size*BLOCK_SIZE)%8 != 0){
		printf("find_free_segment: size is not aligned\n");
	}
#endif
	
	// start the search at its size class
	size_t** bp;
	for(size_t i = get_class(size); i < len_segment_list; ++i){
		bp = (size_t**) metadata[i];
		while((size_t*) bp != S_NULL){
			if(GET_SIZE_USING_BP(bp) >= size){
				return bp;
			}
			bp = ((size_t**) bp[NEXT]);
		}
	}
	return (size_t**) S_NULL;
}


/*
 * remove_from_segment_list - removes a block from the segment list
 */
void remove_from_segment_list(size_t** cur_bp){

	#ifdef DEBUG
		if(!validate_free_segment(cur_bp)){
			printf("remove_from_segment_list: segment to be removed is not valid free segment\n");
			exit(0);
		}
	#endif

	// let the segment we want to remove be called cur, nxt and prv are cur's adjacent segments in the free list
	// if the segment is the last in the list
	if(cur_bp[NEXT] == S_NULL){
		// if the segment is the first in the list
		// this is the only segment in the free list
		if(cur_bp[PREV] == S_NULL){
			metadata[get_class(GET_SIZE_USING_BP(cur_bp))] = S_NULL;
		}
		// this is the last segment in the list with a prev segment
		else{
			size_t** bp_prv = (size_t**) cur_bp[PREV];
			bp_prv[NEXT] = S_NULL;
		}
	}
	// first block in the list
	else if(cur_bp[PREV] == S_NULL){
		metadata[get_class(GET_SIZE_USING_BP(cur_bp))] = cur_bp[NEXT];
		((size_t**) cur_bp[NEXT])[PREV] = S_NULL;
	}
	else{
		size_t** prv_bp = (size_t**) cur_bp[PREV];
		size_t** nxt_bp = (size_t**) cur_bp[NEXT];

		prv_bp[NEXT] = (size_t*) nxt_bp;
		nxt_bp[PREV] = (size_t*) prv_bp;
	}
}


/*
 * insert_into_segment_list - inserts a free block into segment list
 */
void insert_into_segment_list(size_t** bp){

	if(GET_ALLOC_USING_BP(bp)){
		printf("trying to insert invalid block\n");
		exit(0);
	}

	#ifdef DEBUG
		if(!validate_free_segment(bp)){
			printf("insert_into_segment_lis: trying to add invalid block into segment list\n");
			exit(0);
		}
	#endif

	size_t class = get_class(GET_SIZE_USING_BP(bp));
	bp[NEXT] = metadata[class];
	bp[PREV] = S_NULL;
	metadata[class] = (size_t*) bp;

}


/*
 * validate_free_segment - checks if a free block is valid or not
 * returns 1 if it is or else 0
 */
static size_t validate_free_segment(size_t** bp){
	size_t allocation = GET_ALLOC_USING_BP(bp);
	size_t header_size = GET_SIZE_USING_HDR(GET_HDR_USING_BP(bp));
	size_t footer_size = GET_SIZE_USING_FTR(GET_FTR_USING_BP(bp));
	if((header_size == footer_size) && (!allocation)){
		return 1;
	}
	else{
		return 0;
	}
}

/*
 * print_free_list - prints the segment list
 */
void print_free_list(void){

	size_t** bp;
	size_t size;
	size_t alloc;
	size_t alloc_prev;
	size_t validity;

	for(size_t i = 0; i < len_segment_list; ++i){

		bp = (size_t**) metadata[i];
		while((size_t*) bp != S_NULL){

			alloc = GET_ALLOC_USING_BP(bp);
			size = GET_SIZE_USING_BP(bp);
			alloc_prev = GET_PREV_ALLOC_USING_BP(bp);
			validity = validate_free_segment(bp);

			printf("alloc: %zu alloc_prev: %zu size: %zu validity: %zu\n", alloc, alloc_prev, size, validity);

			bp = (size_t**) bp[NEXT];
		}
	}
}

/*
 * mm_init - initialize the malloc package.
 * contract-
 * 1. to be called one time before using malloc
 */
int mm_init(void)
{
	page_size = ALIGN(mem_pagesize());
	mem_init();

	// Create the METADATA Heap, which contains the segment list and an prologue block
	// Segment list is array of size_t* pointing to free blocks
	len_segment_list = BITS;
	size_t no_blocks_metadata = len_segment_list + (HEADER_SIZE/BLOCK_SIZE);
	size_t size_metadata_bytes = no_blocks_metadata * BLOCK_SIZE;

	// Request size_metadata_bytes amount of space from OS (in this case, memlib.c)
	// size_t** metadata points to an array of size_t* 
	metadata = mem_sbrk(size_metadata_bytes);
	heap_start = (size_t*) (((char*) metadata) + size_metadata_bytes);

	// mem_sbrk failed
	if(metadata == (size_t**)-1){
		return -1;
	}

	// Initialize the segmentlist array
	// Initially members of segment point to S_NULL cause there is no free block yet
	for(size_t i = 0 ; i < len_segment_list; ++i){
		metadata[i] = S_NULL; 
	}

	// Get the pointer to the prologue (zero size allocated block)
	size_t* prologue = (size_t*) (((char*) metadata) + size_metadata_bytes - HEADER_SIZE);
	PUT_AS_HEADER_OR_FOOTER(prologue, 0x1);

	/* Code to print the metadata
	for(size_t i = 0; i < len_segment_list; ++i){
		printf("%zu: %p\n", i, metadata[i]);
	}
	printf("prologue: %zu\n", *prologue);
	*/

	return 0;
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
	if(size == 0){
		return (void*) -1;
	}

	size_t sbrk_size = page_size;
	size = ALIGN(size);
	if((size / BLOCK_SIZE) < MIN_BLOCKS_IN_FREE){
		size = MIN_BLOCKS_IN_FREE * BLOCK_SIZE;
	}
	// if we happen to request new memory and if the stuff doesnt fit inside the page
	if((size + ((2 * BLOCKS_IN_ONE_HEADER) * BLOCK_SIZE)) > page_size){
		sbrk_size = size + ((2 * BLOCKS_IN_ONE_HEADER) * BLOCK_SIZE);
	}
	// convert size to no of blocks
	size = size / BLOCK_SIZE;

	size_t** bp = find_free_segment(size);
	// coult NOT find a free block
	if((size_t*) bp == S_NULL){
		
		// request additional memory using mem_sbrk and create a new block + ending epilogue block
		// leave some blocks for the header and the epilogue blocks
		size_t size_new_segment = (sbrk_size / BLOCK_SIZE) - (2 * BLOCKS_IN_ONE_HEADER);
		size_t* header_new_free_segment = mem_sbrk(sbrk_size);
		if((void*) header_new_free_segment == (void*) -1){
			printf("mm_malloc: mem_sbrk failed.\n");
			return (void*) -1;
		}
		size_t** bp_new_free_segment = GET_BP_USING_HDR(header_new_free_segment);


		// create_free_segment preserves the allocation variable which is not desirable
		// manually change the allocation variable to be 1, cause the prev segment is always either
		// prologue or epilogue
		create_free_segment(bp_new_free_segment, size_new_segment);
		*header_new_free_segment = *header_new_free_segment | 0x2;

		// create the epilogue block
		size_t* epilogue = GET_NXT_HDR_USING_BP(bp_new_free_segment);
		// epilogue is a zero size allocated block to fix the coaless end conditions
		// allocation variable is 0 cause prv block is free
		*epilogue = 0x1;

		#ifdef DEBUG
			if(!validate_free_segment(bp_new_free_segment)){
				printf("mm_malloc: newly created big free segment is not valid\n");
				exit(0);
			}
		#endif

		// split the block into two, and allocate the main block
		size_t** leftover = split_free_segment(bp_new_free_segment, size);
		// split was successful
		if((size_t*) leftover != S_NULL){
			insert_into_segment_list(leftover);
		}

		// we split the block so the prev bp serves as the bp for main and the leftover is the bp for the leftover block
		bp = bp_new_free_segment;

		#ifdef DEBUG
			if(!validate_free_segment(bp)){
				printf("mm_malloc: main segment from the new big free segment is not valid\n");
				exit(0);
			}
		#endif

		allocate_segment(bp);
		return (void*) bp;
	}

	// bp is a free segment in the free segment list
	// first remove the bp from the segment list
	size_t validate = validate_free_segment(bp);
	size_t alloc_status = GET_ALLOC_USING_BP(bp);
	remove_from_segment_list(bp);
	//try to split bp as to save space
	size_t** extra_segment = split_free_segment(bp, size);
	// split was successful
	if((size_t*) extra_segment != S_NULL){
		insert_into_segment_list(extra_segment);
	}
	allocate_segment(bp);
	return (void*) bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
	size_t** bp = (size_t**) ptr;
	size_t alloc = GET_ALLOC_USING_BP(bp);
	if(!alloc){
		printf("mm_free: trying to free a free block\n");
		exit(0);
	}
	size_t size = GET_SIZE_USING_BP(bp);
	create_free_segment(bp, size);

	// we have a segment not in a free list, try to coalesce
	//size_t** new_bp = coalesce(bp);
	insert_into_segment_list(bp);

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
}

/*
int main(void){
}
*/










