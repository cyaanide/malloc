#include <stdio.h>

extern int mm_init (void);
extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);


// Macro constants -----------------------------------------------------------

// define debug mode

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

// each payload block is of BLOCK_SIZE
#define BLOCK_SIZE (sizeof(size_t*))

// block header size
#define HEADER_SIZE (ALIGN(sizeof(size_t)))

// bits in a size_t* on a system
#define BITS 8 * (sizeof(size_t*))

// Array indices for next and prev size_t*
#define NEXT 0
#define PREV 1

// Min blocks in a free block
// 4 cause 2 for ptrs and one for footer, one for aligment on 32 bit systems
#define MIN_BLOCKS_IN_FREE 4

#define BLOCKS_IN_ONE_HEADER HEADER_SIZE/BLOCK_SIZE

// Macro function definitions -------------------------------------------------

// rounds up to the nearest multiple of ALIGNMENT  (checked)
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

// put value as a header/footer value given the header/footer pointer
#define PUT_AS_HEADER_OR_FOOTER(p, val) (*((size_t*)p) = (val))

// get header using base pointer (checked)
#define GET_HDR_USING_BP(bp) ((size_t*) ((((char*) ((size_t**)(bp))) - HEADER_SIZE)))

// get header using footer (checked)
#define GET_HDR_USING_FTR(footer) GET_HDR_USING_BP(GET_BP_USING_FTR((footer)))

// get base pointer from header (checked)
#define GET_BP_USING_HDR(header) ((size_t**) (((char*) (header)) + HEADER_SIZE))

// get base pointer using footer (checked)
#define GET_BP_USING_FTR(footer) ((size_t**) (((size_t**) (footer)) - GET_SIZE_USING_FTR(footer) + 1))

// get footer using bp (checked)
// only works when header has valid size
#define GET_FTR_USING_BP(bp) ((size_t*) (((size_t**) (bp)) + GET_SIZE_USING_BP((bp)) - 1))

// get footer using header (checked)
// only workd when header has valid size
#define GET_FTR_USING_HDR(header) GET_FTR_USING_BP((size_t**) GET_BP_USING_HDR((size_t*) (header)))

// get size using base pointer (checked)
#define GET_SIZE_USING_BP(bp) ((*(GET_HDR_USING_BP((size_t**) (bp))) & ~(0x7)) / BLOCK_SIZE)

// get size using header (checked)
#define GET_SIZE_USING_HDR(header) (((*((size_t*) (header)) & ~(0x7)) / BLOCK_SIZE))

// get size using footer (checked)
#define GET_SIZE_USING_FTR(footer) (((*((size_t*) (footer)) & ~(0x7)) / BLOCK_SIZE))

// get allocation using bp (checked)
#define GET_ALLOC_USING_BP(bp) (*(GET_HDR_USING_BP((size_t**) (bp))) & 0x1)

// get allocation using header (checked)
#define GET_ALLOC_USING_HDR(header) ((*((size_t*) (header))) & 0x1)

// get prev's footer using current's base pointer (checked)
#define GET_PREV_FTR_USING_BP(bp) ((size_t*) (((size_t**) (GET_HDR_USING_BP((bp)))) - 1))

// get next header using current bp (checked)
#define GET_NXT_HDR_USING_BP(bp) ((size_t*) (((size_t**) (bp)) + GET_SIZE_USING_BP((size_t**)(bp))))

// get next bp using current bp
#define GET_NXT_BP_USING_BP(bp) GET_BP_USING_HDR(GET_NXT_HDR_USING_BP((bp)))

// get prev allocation using bp (checked)
#define GET_PREV_ALLOC_USING_BP(bp) (*((size_t*) GET_HDR_USING_BP((size_t**) (bp))) & 0x2)

// get prev allocation using header (checked)
#define GET_PREV_ALLOC_USING_HDR(header) (*((size_t*) (header)) & 0x2)

// get next allocation using bp
#define GET_NEXT_ALLOC_USING_BP(bp) ((*((size_t*) (((size_t**) (bp)) + ((size_t) GET_SIZE_USING_BP((bp)))))) & ((size_t) 0x1))

// get next allocation using header
#define GET_NEXT_ALLOC_USING_HDR(header) GET_NEXT_ALLOC_USING_BP(GET_BP_USING_HDR((header)))

// global variables -----------------------------------------------------------

static size_t** metadata;
static size_t len_segment_list;
static size_t page_size;

// function prototypes --------------------------------------------------------

static size_t get_class(size_t size);
static void create_free_segment(size_t** bp, size_t size);
static size_t** split_free_segment(size_t** bp, size_t size);
void allocate_segment(size_t** bp);
static size_t** find_free_segment(size_t size);
void remove_from_segment_list(size_t** cur_bp);
void insert_into_segment_list(size_t** bp);
static size_t validate_free_segment(size_t** bp);
void print_free_list(void);
