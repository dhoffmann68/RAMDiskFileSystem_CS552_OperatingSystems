/*


Darren Hoffmann-Marks
Ilko Mihov

*/





#include "multiboot.h"
#include "types.h"


/*************************************************************
*       Start of Test Defines
*************************************************************/
#define TEST1
#define TEST2
#define TEST3
#define TEST4
#define TEST5
#define TEST6
#define USE_RAMDISK

// File modes
#define RD  1
#define RW  0
#define WR  2

// File open flags
#define READONLY   1
#define READWRITE  0
#define WRITEONLY  2

// Insert a string for the pathname prefix here. For the ramdisk, it should be
// NULL
//#define PATH_PREFIX 0

#ifdef USE_RAMDISK
#define CREAT   rd_create
#define OPEN    rd_open
#define WRITE   rd_write
#define READ    rd_read
#define UNLINK  rd_unlink
#define MKDIR   rd_mkdir
#define CLOSE   rd_close
#define LSEEK   rd_lseek
#define CHMOD   rd_chmod

#else
#define CREAT   creat
#define OPEN    open
#define WRITE   write
#define READ    read
#define UNLINK  unlink
#define MKDIR(path)   mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
#define CLOSE   close
#define LSEEK(fd, offset)   lseek(fd, offset, SEEK_SET)
#define CHMOD  chmod
#endif

#define TEST_SINGLE_INDIRECT
#define TEST_DOUBLE_INDIRECT


#define MAX_FILES 1023
#define BLK_SZ 256		/* Block size */
#define DIRECT 8		/* Direct pointers in location attribute */
#define PTR_SZ 4		/* 32-bit [relative] addressing */
#define PTRS_PB  (BLK_SZ / PTR_SZ) /* Pointers per index block */

static char pathname[80];

static char data1[DIRECT*BLK_SZ]; /* Largest data directly accessible */
static char data2[PTRS_PB*BLK_SZ];     /* Single indirect data size */
static char data3[PTRS_PB*PTRS_PB*BLK_SZ]; /* Double indirect data size */
static char addr[PTRS_PB*PTRS_PB*BLK_SZ+1]; /* Scratchpad memory */



/*************************************************************
*       End of Test Defines
*************************************************************/


#define startOfRAMdisk 0x600000
#define startSuperBlock 0x600000
#define startINodeArray 0x600100
#define startBitMap 0x610100
#define startFiles 0x610500
#define totalBlocks  7931
#define totalINodes 1024
#define MAX_FILES_IN_FDT 1024
#define startFDTs 0x800000
#define Sbuffer 0x900000



/* Hardware text mode color constants. */
enum vga_color
{
  COLOR_BLACK = 0,
  COLOR_BLUE = 1,
  COLOR_GREEN = 2,
  COLOR_CYAN = 3,
  COLOR_RED = 4,
  COLOR_MAGENTA = 5,
  COLOR_BROWN = 6,
  COLOR_LIGHT_GREY = 7,
  COLOR_DARK_GREY = 8,
  COLOR_LIGHT_BLUE = 9,
  COLOR_LIGHT_GREEN = 10,
  COLOR_LIGHT_CYAN = 11,
  COLOR_LIGHT_RED = 12,
  COLOR_LIGHT_MAGENTA = 13,
  COLOR_LIGHT_BROWN = 14,
  COLOR_WHITE = 15,
};
 
uint8_t make_color(enum vga_color fg, enum vga_color bg)
{
  return fg | bg << 4;
}
 
uint16_t make_vgaentry(char c, uint8_t color)
{
  uint16_t c16 = c;
  uint16_t color16 = color;
  return c16 | color16 << 8;
}
 
size_t strlen(const char* str)
{
  size_t ret = 0;
  while ( str[ret] != 0 )
    ret++;
  return ret;
}
 
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 24;
 
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;
 
void terminal_initialize()
{
  terminal_row = 0;
  terminal_column = 0;
  terminal_color = make_color(COLOR_LIGHT_GREY, COLOR_BLACK);
  terminal_buffer = (uint16_t*) 0xB8000;
  for ( size_t y = 0; y < VGA_HEIGHT; y++ )
    {
      for ( size_t x = 0; x < VGA_WIDTH; x++ )
	{
	  const size_t index = y * VGA_WIDTH + x;
	  terminal_buffer[index] = make_vgaentry(' ', terminal_color);
	}
    }
}
 
void terminal_setcolor(uint8_t color)
{
  terminal_color = color;
}
 
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
  const size_t index = y * VGA_WIDTH + x;
  terminal_buffer[index] = make_vgaentry(c, color);
}
 
void terminal_putchar(char c)
{
  terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
  if ( ++terminal_column == VGA_WIDTH )
    {
      terminal_column = 0;
      if ( ++terminal_row == VGA_HEIGHT )
	{
	  terminal_row = 0;
	}
    }
}
 
void terminal_writestring(const char* data)
{
  size_t datalen = strlen(data);
  for ( size_t i = 0; i < datalen; i++ )
    terminal_putchar(data[i]);
}
 
 //Function used for testing purposes
 void write_timer(void){
	terminal_writestring("    timer    "); 
	return;
 }



/* Convert the integer D to a string and save the string in BUF. If
   BASE is equal to 'd', interpret that D is decimal, and if BASE is
   equal to 'x', interpret that D is hexadecimal. */
void itoa (char *buf, int base, int d)
{
  char *p = buf;
  char *p1, *p2;
  unsigned long ud = d;
  int divisor = 10;
     
  /* If %d is specified and D is minus, put `-' in the head. */
  if (base == 'd' && d < 0)
    {
      *p++ = '-';
      buf++;
      ud = -d;
    }
  else if (base == 'x')
    divisor = 16;
     
  /* Divide UD by DIVISOR until UD == 0. */
  do
    {
      int remainder = ud % divisor;
     
      *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    }
  while (ud /= divisor);
     
  /* Terminate BUF. */
  *p = 0;
     
  /* Reverse BUF. */
  p1 = buf;
  p2 = p - 1;
  while (p1 < p2)
    {
      char tmp = *p1;
      *p1 = *p2;
      *p2 = tmp;
      p1++;
      p2--;
    }
}

void terminal_writeint(int num){
    char lcStr[30];
    itoa(lcStr, 'd', num);
    terminal_writestring(lcStr);
    return;
}

void init( multiboot* pmb ) {

   memory_map_t *mmap;
   unsigned int memsz = 0;		/* Memory size in MB */
   static char memstr[10];

  for (mmap = (memory_map_t *) pmb->mmap_addr;
       (unsigned long) mmap < pmb->mmap_addr + pmb->mmap_length;
       mmap = (memory_map_t *) ((unsigned long) mmap
				+ mmap->size + 4 /*sizeof (mmap->size)*/)) {
    
    if (mmap->type == 1)	/* Available RAM -- see 'info multiboot' */
      memsz += mmap->length_low;
  }

  /* Convert memsz to MBs */
  memsz = (memsz >> 20) + 1;	/* The + 1 accounts for rounding
				   errors to the nearest MB that are
				   in the machine, because some of the
				   memory is othrwise allocated to
				   multiboot data structures, the
				   kernel image, or is reserved (e.g.,
				   for the BIOS). This guarantees we
				   see the same memory output as
				   specified to QEMU.
				    */

  itoa(memstr, 'd', memsz);

  terminal_initialize();

  //terminal_writestring("MemOS: Welcome *** System memory is: ");
  //terminal_writestring(memstr);
  //terminal_writestring("MB");

}




#define MAX_THREADS 10

typedef struct runqueue {
  struct TCB * thread;
  struct runqueue *next;
  struct runqueue *prev;
} rq;

static rq head;
static int done[MAX_THREADS];

//do I need an array of threads that are done

struct TCB {
  void (*task)(struct TCB *);
  int tid;
  void * stack_b;
  void * stack_top;
  void * isp;
  int busy;
  int preempt;
  int priority;

  //added for DISCOS
  struct File_Object * FDT_ptr;
};


struct TCB * TCBs[MAX_THREADS];
//struct TCB * TCBs;

// this is an array of function pointers that don't take any parameters
int (*f[MAX_THREADS])(int);



extern void yield(struct TCB * tcb);


void threadFunction(struct TCB * tcb){


  char tidStr[10];
  int tid = tcb->tid;
  itoa(tidStr, 'd', tid);
  terminal_writestring("<");
  terminal_writestring("Running");
  


  for (int i = 0; i < 10000000; i++)
  {
		if ((i % 5000) == 0)
		{
			terminal_writestring("TEST <");
			terminal_writestring(tidStr);
			terminal_writestring(">");
			
		}
  }

  terminal_writestring("Thread");
  terminal_writestring(tidStr);
  terminal_writestring(">");

  terminal_writestring("Done <");
  terminal_writestring(tidStr);
  terminal_writestring(">");
  done[tid] = 1;
  tcb->busy = 0;

  return;
}


/*
On first call to addToQueue, obviously the queue is empty,
so head contains {thread = 0x0, next = 0, prev = 0} and doesn't 
enter the while loop. So then it sets the current (head) to contain
a pointer to the TCB[0] structure
  {thread = 0x105500, next = 0x0, prev= 0x0}

*/
void addToQueue(int tid)
{

  rq *ptr, *pptr;
  rq * current = &head;
  int i = 1;
  while (current->thread != 0)
  {

    ptr = (rq *) (&head + (i*(sizeof(rq))));
    if (i == 1)
    {
        current->next = ptr;
        pptr = &head;
    }
    else {
      pptr = pptr->next;
      pptr->next = ptr;
    }

    ptr->prev = pptr;
    ptr->next = &head; // Wraparound
    head.prev = ptr;
    current = ptr;
    i++;
  }

  //current->thread = TCBs[tid];
  current->thread = TCBs[tid];
  return;
}




/**********************************
 * Only creates one thread
 * 
 *  - need to make it so that it takes in a stack pointer
 * 
 *********************************/
int thread_create(void (*func)(struct TCB *), void * stackStart)
{
  for (int i = 0; i < MAX_THREADS; i++)
  {   
      if (TCBs[i]->busy == 0)
      {
          //looks good
          TCBs[i]->task = func;
          TCBs[i]->busy = 1;
          TCBs[i]->stack_b = stackStart;
          TCBs[i]->stack_top = stackStart;

          addToQueue(i);
          return i;
      }
  }
  return 1000;
}

extern void switch_context(struct TCB * tcb);
extern void restore_context(void);
extern void resume_thread(struct TCB * tcb);
extern void runThread(struct TCB * tcb);



//doesn't take care of stacks
void initializeThreads(void){
  int startAddress = 0x10a500;
  for (int i = 0; i < MAX_THREADS; i++)
  {
    TCBs[i] = (struct TCB *) (startAddress + (i * sizeof(struct TCB)));
    TCBs[i]->tid = i;
    TCBs[i]->busy = 0;
    TCBs[i]->preempt = 0;
    TCBs[i]->priority = 0;
    TCBs[i]->FDT_ptr = (struct File_Object *)(startFDTs + (i * MAX_FILES_IN_FDT * 8));
  }
}



void schedule(void)
{
  rq * current;
  rq *finished;

  int threads = MAX_THREADS;

  current = &head;

  // current is set to an address, the address of head
  while(current)
  {
    int tid = current->thread->tid;   //works correctly




    runThread(current->thread);


    if(done[tid])
    {
      if (threads == 1)
      {
        return;
      }
      finished = current;
      finished->prev->next = current->next;
      current = current->next;
      current->prev = finished->prev;

      if (current->next == finished) 
      { // Down to last thread
	        current->next = finished->next;
      }
      threads--;
    }
    else
    {
      current = current->next;
    } 
  }
}


































//TODO 1: Check block allocation policy
/*
    Block Allocation policy:
        - when an operation fills up a block, even if it doens't need anymore space, add
          a new block to the file
        - only allocate a new block if a block is completely full
*/
//TODO 2: Check block size policy
/*
    - size: represent the number of valuable bytes to read
    - numBlocks: the number of blocks the file is using
*/


struct  iNode{
    // 1 means iNode is in use
    int inUse;
    // 0 for directory, 1 for regular file
    int type;
    //number of valuable bytes in the file
    int size;
    //array of block pointers
    void * location[10];
    // read-write = 0, read-only = 1, write-only = 2
    int rights;
    int usedBy;
    int numBlocks;

};

struct superblock
{
    int remainingBlocks;
    int remainingInodes;
};


struct File_Object{
    int pos;
    struct iNode * ptr;
};

struct superblock * sb = (struct superblock *) startSuperBlock;





unsigned int checkDirectoryEntries(struct iNode * inode, char * name);
int strLen(char * pathname);
int getDirectoriesStrLength(char * pathname);
int getNumDirectoriesInPath(char * pathname);
int getDirectoryNameLen(char * pathname, int num);
struct iNode * getNewFileDirectoryInode(char * pathname);
unsigned int calculateInodeAddress(short index);
void * calculateBlockAddress(int BMByte, int byteBit);
void * findAndSetBlock();
int calculateiNodeIndex(unsigned int inodeAddr);
int findAndSetiNode(int type, int rights);
int compareNames(char * name1, char * name2);
unsigned int checkBlock(unsigned int blockPointer, char * name);
unsigned int checkSingleIndirectPointer(unsigned int blockPointer, char * name);
unsigned int checkDoubleIndirectPointer(unsigned int blockPointer, char * name);
unsigned int checkDirectoryEntries(struct iNode * inode, char * name);
void * getAppendToAddress(struct iNode * inode, int size);
int addDirectoryEntry(struct iNode * directoryiNode, char * filename, int fileiNodeNumber);
void findAndSetBlockToNull(unsigned int blockAddr);




void findAndSetBlockToNull(unsigned int blockAddr){
    unsigned int blockIndex = blockAddr - startFiles;
    //get block number
    blockIndex /= 256;
    unsigned char * BMp = (unsigned char *) ((unsigned int) startBitMap + blockIndex/8);
    *BMp = ((*BMp) & (~(1<<(blockIndex%8))));   // the AND will be with a number that contains all 0s except at the place that we need to Nullify

    //TODO check that block was used in the first place
    sb->remainingBlocks++;
    return;
}


int strLen(char * pathname)
{
	int size = 0;
	char * temp = pathname;
	while(1)
	{
		if (*temp != '\0')
		{
			size++;
			temp++;
		}
		else
		{
			break;
		}
	}
	return size;

}


/*
	Add by this length to get where filename starts
*/
int getDirectoriesStrLength(char * pathname)
{
	//size of the pathname
	int size = strLen(pathname);
	// temp is set to last character in string
	char * temp = pathname + (size - 1);
	// after this while loop, temp will be pointing to start of filename
	while(*(temp-1) != '/')	{
	    temp--;
    }

	return (int)(temp - pathname);
}

int getNumDirectoriesInPath(char * pathname)
{
	int count = 0;
	while (*pathname != '\0')
	{
		if (*pathname == '/')
			count ++;
		pathname++;
	}
	return count;
}

/*
    Get the length of the num'th Directory name after the root directory
*/
int getDirectoryNameLen(char * pathname, int num)
{
    char * lastSlash = pathname;
    int count = -1;
    while (*pathname != '\0')
    {
        if (*pathname == '/')
        {
            count++;
            if (count == num)
        	{
            	return (int)((pathname - lastSlash) - 1);
        	}
        	lastSlash = pathname;
        }
        pathname++;
    }

    return -1;
}


/*
    Allocates a new block for the file. If it can't, return 0; otherwise,
    return address of new block
    TODO: increment numBlocks when we add a new FILE block
    TODO: Free blocks using findAndSetBlockNULL() if failed.

*/
unsigned int allocateBlock(struct iNode * inode)
{
    if (inode->numBlocks == 4168)
    {
        //file has maximum blocks
        terminal_writestring("File already has max blocks");
        return 0;
    }

    void * newBlockAddr = findAndSetBlock();
    if (!(newBlockAddr))
    {
        terminal_writestring("Couldn't allocate new block");
        return 0;
    }

    //if there are less than 8 blocks, insert a new direct pointer
    if(inode->numBlocks < 8)
    {
        inode->location[inode->numBlocks] = newBlockAddr;
        inode->numBlocks++;
        return ((unsigned int)newBlockAddr);
    }
    else if (inode->numBlocks == 8)
    {
        void * newSIPblockAddr = findAndSetBlock();
        if(!(newSIPblockAddr))
        {
            findAndSetBlockToNull((unsigned int)newBlockAddr);
            terminal_writestring("Couldn't allocate new SIP block");
            return 0;
        }
        inode->location[8] = newSIPblockAddr;
        *((unsigned int *)newSIPblockAddr) = (unsigned int)newBlockAddr;
        inode->numBlocks++;
        return ((unsigned int)newBlockAddr);
    }
    else if (inode->numBlocks < 72)
    {
        //insert new direct pointer in block pointed to by SIP
        unsigned int SIPblockAddr = (unsigned int)inode->location[8];
        unsigned int insertOffset = (inode->numBlocks - 8) * 4;
        *((unsigned int *)(SIPblockAddr + insertOffset)) = (unsigned int)newBlockAddr;
        inode->numBlocks++;
        return ((unsigned int)newBlockAddr);
    }
    else if (inode->numBlocks == 72)
    {
        void * newDIPblockAddr = findAndSetBlock();
        if(!(newDIPblockAddr))
        {
            findAndSetBlockToNull((unsigned int)newBlockAddr);
            terminal_writestring("Couldn't allocate new DIP block");
            return 0;
        }
        void * newSIPblockAddr = findAndSetBlock();
        if(!(newSIPblockAddr))
        {
            findAndSetBlockToNull((unsigned int)newBlockAddr);
            findAndSetBlockToNull((unsigned int)newDIPblockAddr);
            terminal_writestring("Couldn't allocate new SIP for new DIP block");
            return 0;
        }
        inode->location[9] = newDIPblockAddr;
        *((unsigned int *)newDIPblockAddr) = (unsigned int)newSIPblockAddr;
        *((unsigned int *)newSIPblockAddr) = (unsigned int)newBlockAddr;
        inode->numBlocks++;
        return ((unsigned int)newBlockAddr);
    }
    else
    {

        unsigned int DIPblockAddr = (unsigned int)inode->location[9]; //

        //SIPindex of where new block is suppossed to go
        unsigned int SIPindex = (inode->numBlocks - 72) / 64;

        unsigned int SIPblockAddr = *((unsigned int *)(DIPblockAddr + (SIPindex * 4)));
        //if that SIP block hasn't been made yet
        if (!(SIPblockAddr))
        {
            if(SIPindex == 64)
            {
                findAndSetBlockToNull((unsigned int)newBlockAddr);
                terminal_writestring("File already has max SIP pointers");
                return 0;
            }

            //SIP pointer not set, create block for SIP and set SIP pointer to that block
            unsigned int newSIPblockAddr = (unsigned int)findAndSetBlock();
            if(!(newSIPblockAddr))
            {
                findAndSetBlockToNull((unsigned int)newBlockAddr);
                terminal_writestring("Couldn't allocate new SIP block");
                return 0;
            }
            SIPblockAddr = newSIPblockAddr;
            *((unsigned int *)(DIPblockAddr + (SIPindex * 4))) = SIPblockAddr;
        }
        unsigned int dirBlockIndex = (inode->numBlocks - 72) % 64;
        //set the new direct pointer value
        *((unsigned int *)(SIPblockAddr + (dirBlockIndex * 4))) = (unsigned int)newBlockAddr;
        inode->numBlocks++;
        return ((unsigned int)newBlockAddr);;
    }



}













/*

*/
struct iNode * getNewFileDirectoryInode(char * pathname)
{
    //this checks out
    int numDirectories = getNumDirectoriesInPath(pathname);

    struct iNode * directoryInode = (struct iNode *) startINodeArray;
    //if there is only the root directory in the pathname
    if (numDirectories == 1)
    {
        return directoryInode;
    }


    //loop through the number of directories
    //i is set to 1 because we skip the root directory
    for (int i = 1; i < numDirectories; i++)
    {
        char * pathPointer = pathname;
        //gets the directory name length of the ith directory after the root
        int nameLen = getDirectoryNameLen(pathname, i);
        char dirName[nameLen+1];

        int count = 0;
        while (count < i)
        {
            //increments count if we've found the start of the ith directory name
            if (*pathPointer == '/')
                count++;
            pathPointer++;

            //if we're at the start of the ith directory name
            if (count == i)
            {
                for (int z = 0; z < nameLen; z++)
                {
                    dirName[z] = *pathPointer;
                    pathPointer++;
                }
                dirName[nameLen] = '\0';

                /*
                    We have the directory name held within the variable dirName. We have
                    to find the directory entry with the same directory name in the current
                    directory. If we don't find it, return false
                */
                //gets the address of the directory entry corresponding to dirName
                //TODO: (finished) Check if checkDirectoryEntries works correctly
                unsigned int dirEntryAddr = checkDirectoryEntries(directoryInode, dirName);

                //if an entry was found corresponding to the directory name 'dirName'
                if(dirEntryAddr)
                {
                    short dirINodeIndex = *(short *)(dirEntryAddr + 14);
                    //calculate the address of the iNode
                    //then set the directoryInode to that address
                    directoryInode = (struct iNode *)calculateInodeAddress(dirINodeIndex);
                }
                else
                {
                    terminal_writestring("The Directory was not found within current directory");
                    return 0;
                }

            }
        }
    }
    return directoryInode;

}



//returns the address of the 'blockNum'th block (0 based indexing)
//of the file referenced to by the inode
unsigned int getBlockAddress(struct iNode * inode, int blockNum)
{
    //check if that blockNum index is greater than the nnu
    if (inode->numBlocks <= blockNum)
    {
        //terminal_writestring("File doesn't have that many blocks");
        return 0;
    }

    if (blockNum < 8)
    {
        return (unsigned int)inode->location[blockNum];

    }
    else if (blockNum < 72)
    {
        //get block address from list of direct pointers referened by the SIP
        unsigned int SIPblockAddr = (unsigned int)inode->location[8];
        int dirPIndex = blockNum - 8;
        unsigned int blockAddr = *((unsigned int *)(SIPblockAddr + (dirPIndex * 4)));

        return blockAddr;

    }
    else
    {
        unsigned int DIPblockAddr = (unsigned int)inode->location[9];
        unsigned int SIPindex = ((blockNum - 72) / 64);

        unsigned int SIPblockAddr = *((unsigned int *)(DIPblockAddr + (SIPindex * 4)));
        unsigned int dirBlockIndex = ((blockNum - 72) % 64);

        unsigned int blockAddr = *((unsigned int *)(SIPblockAddr + (dirBlockIndex * 4)));

        return blockAddr;

    }
    return 0;
}











unsigned int calculateInodeAddress(short index)
{
    return (unsigned int)(startINodeArray + (index * 64));
}

//take in the index of the byte within the bitmap, and
//the index of the bit within that byte. Returns the
//corresponding block's address
void * calculateBlockAddress(int BMByte, int byteBit)
{
    int offset = (BMByte * 8) + byteBit;

    return (void *)(startFiles + (offset * 256));
}


/**********************************************************
finished
********************************************************** */
//This function will go through the block bitmap, bit by bit
//until it finds a bit set to 0. Then set that bit to 1 and
//returns the memory address of that block. If it can't find
//an available block, it returns 0
//TODO: only in findAndSetBlock() and findAndSetBlockNULL() do we adjust
//TODO (Cont): the number of remaining blocks
void * findAndSetBlock()
{
    unsigned char * BMp = (unsigned char *)startBitMap;
    if (sb->remainingBlocks == 0)
        return 0;

    //loop through all the bytes of the bitmap
    for (int i = 0; i < 1024; i++)
    {
        for (int n = 0; n < 8; n++)
        {
            // check if nth bit of the byte is set to 0
            if (!(*BMp & (1 << n)))
            {   //set the bit to 1
                *BMp = (*BMp | (1 << n));
                sb->remainingBlocks--;
                return calculateBlockAddress(i, n);
            }
        }
        BMp++;
    }
    //terminal_writestring("No Free Blocks Left");
    return 0;
}


int calculateiNodeIndex(unsigned int inodeAddr)
{
    return (int)((inodeAddr - startINodeArray) / 64);

}

//takes in the type of the file and the rights. Then
//searches through the iNode array to find an unused
//iNode, set its values, and returns its index
int findAndSetiNode(int type, int rights)
{
    struct iNode * inode = (struct iNode *)startINodeArray;

    //loop through iNode array until we find an iNode that isn't in use
    while(inode->inUse == 1){
        //terminal_writestring("      ENTERED THE LOOP     ");
        inode = (struct iNode *)(64 + ((unsigned int)inode));
        if ((unsigned int)inode >= startBitMap)
            return -1;
    }

    //TODO
    void * startBlockAddr = findAndSetBlock();
    if (!(startBlockAddr))
    {
        return -1;
    }

    inode->inUse = 1;
    inode->type = type;
    inode->size = 0;
    inode->location[0] = startBlockAddr;
    inode->rights = rights;
    inode->usedBy = 0;
    inode->numBlocks = 1;

    return calculateiNodeIndex((unsigned int)inode);


}





/*******************************************************************


            This Section Deals With Checking Directory Entries
                            For a Filename

*******************************************************************/

//compares two filenames. Returns 1 if they match
int compareNames(char * name1, char * name2)
{
    while(*name1 != '\0' && *name2 != '\0')
    {
        if (*name1 != *name2)
            return 0;
        name1++;
        name2++;
    }
    if(*name1 == *name2)
        return 1;
    else
        return 0;
}

/*
    Returns address of matching directory entry. 0 otherwise.
*/
unsigned int checkBlock(unsigned int blockPointer, char * name)
{
    //there are 16 entries to check
    for (unsigned int i = 0; i < 16; i++)
    {
        //returns address of directory entry that matches the name
        if(compareNames(((char *)(blockPointer +(i *16))), name))
            return (blockPointer +(i *16));
    }
    return 0;

}


//blockPointer points to the block containing the 64 direct pointers
//loops through all the block pointers and checks each block for
//a directory entry matching the name. If a match is found, the
//address of that directory entry is returned
unsigned int checkSingleIndirectPointer(unsigned int blockPointer, char * name)
{
    unsigned int temp = 0;
    for (unsigned int i = 0; i < 64; i++)
    {
        temp = checkBlock(*((unsigned int *)(blockPointer + (i * 4))), name);
        if (temp)
            return temp;
    }
    return 0;
}


//blockPointer points to the block containing the 64 single indirect pointers,
//loops through all the block pointers and checks each single indirect pointer
unsigned int checkDoubleIndirectPointer(unsigned int blockPointer, char * name)
{
    unsigned int temp = 0;
    for (unsigned int i = 0; i < 64; i++)
    {
        temp = checkSingleIndirectPointer(*((unsigned int *)(blockPointer + (i * 4))), name);
        if (temp)
            return temp;
    }
    return 0;
}


/*
Check if a directory entry within a directory file contains the same filename.
If it does, then return the address of that directory entry
*/
unsigned int checkDirectoryEntries(struct iNode * inode, char * name)
{
    //total blocks containing the directory file
    //int totalBlocksToCheck = (inode->size / 256) + 1;
    int totalBlocksToCheck = inode->numBlocks;

    unsigned int temp = 0;
    //loop through all the blocks to check
    for (int i = 0; i < totalBlocksToCheck; i++)
    {
        //check if its a direct block pointer
        if (i < 8)
        {
            //check block of directory entries for name
            temp = checkBlock((unsigned int)inode->location[i], name);
            if (temp)
                return temp;
        }
        else
        {
            //check all 64 blocks pointed to within the single indirect pointer's block
            if (i < 72)
            {
                temp = checkSingleIndirectPointer((unsigned int)inode->location[8], name);
                if (temp)
                    return temp;
            }
            else    // check all 64^2 blocks within the doubel indirect pointer's block
            {
                temp = checkDoubleIndirectPointer((unsigned int)inode->location[9], name);
                if (temp)
                    return temp;
            }
        }
    }
    return 0;
}





int checkFreeSIPBlock(unsigned int * SIPp)
{
    int count = 0;
    for (int i = 0; i < 64; i++)
    {
        if (*SIPp == 0)
        {
            count++;
        }
        SIPp = (unsigned int *)((unsigned int)SIPp + 4);
    }

    if (count == 64)
        return 1;
    return 0;

}





// calculates which address to append an object of size <size>
// will allocate another block to file if needed
// returns 0 if it fails
//TODO appropriately update inode->size with each new block allocation
void * getAppendToAddress(struct iNode * inode, int size)
{
    // how many blocks are being used
    //TODO (1) (2)
    int blocksUsed = inode->numBlocks;
    //offset into last block
    int offset = (inode->size % 256);

    if (blocksUsed <= 8)
    {
        //check to see if there isn't enough space in the last block
        //allocate new block if there isn't (case 2)
        if (offset > ((256 - size)))
        {
            //return -1 if we couldn't allocate another block
            void * newBlockAddr = findAndSetBlock();
            if (!newBlockAddr)
                return 0;

            //if there is an unused direct pointer
            if (blocksUsed < 8)
                inode->location[blocksUsed] = newBlockAddr;
            else if(blocksUsed == 8)
            {
                //creates a block for the single indirect pointer
                void * SIndP = findAndSetBlock();
                if(!SIndP)
                    return 0;
                //sets the location pointer for the single indirect pointer
                inode->location[blocksUsed] = SIndP;

                *(unsigned int *)SIndP = (unsigned int)newBlockAddr;
            }
            return newBlockAddr;
        }
        else
        {
            //TODO case 1?
            //case 3
            if (!(inode->location[blocksUsed-1]))
            {
                terminal_writestring("Location Pointer not set!!\n");
                return 0;
            }

            return (void *)((unsigned int)inode->location[blocksUsed-1] + offset);
        }
    }
    else if (blocksUsed <= 72)
    {
        /*
            Check to see if we need to allocate a new block. If we need to
            and the blocks used are already 72, then we need to allocate a
            block for the double indirect pointer, allocate a block for a single
            indirect pointer, and a block for the entry to be written to
        */
        if (offset > ((256 - size)))
        {
            //allocate new block for directory entry
            //return 0 if block can't be allocated
            void * newBlockAddr = findAndSetBlock();
            if (!newBlockAddr)
                return 0;

            if (blocksUsed < 72)
            {
                //the single indirect pointer's block has less than 64 direct pointers
                int numDirectPointers = blocksUsed - 8;
                //need to insert a direct pointer into SIP's block
                int SIPoffset = numDirectPointers * 4;
                *(unsigned int *)((int)inode->location[8] + SIPoffset) = (unsigned int) newBlockAddr;
            }
            if (blocksUsed == 72)
            {
                //set value for double indirect pointer
                //return 0 if block can't be allocated
                void * DoubIndP = findAndSetBlock();
                if (!(DoubIndP))
                    return 0;
                inode->location[9] = DoubIndP;

                // the double indirect pointer's block now needs a single indirect pointer
                void * SIP = findAndSetBlock();
                if(!(SIP))
                    return 0;
                *(unsigned int *)DoubIndP = (unsigned int) SIP;

                //the single indirect pointer's block now needs a pointer to the block
                //that will contain the directory entry
                *(unsigned int *)SIP = (unsigned int) newBlockAddr;
            }
            return newBlockAddr;
        }
        else
        {
            /*
                If there is enough space in the last block, need to find the last block pointer in
                the block of direct pointers pointed to by the single indirect pointer. Return the
                address of the last block plus the offset
            */
            //number of direct pointers in the block pointed to by the single indirect pointer
            int numDirectPointers = blocksUsed - 8;
            int dirPAddr = ((unsigned int)inode->location[8] + ((numDirectPointers - 1) * 4));
            return (void *)(dirPAddr + offset);
        }
    }
    else if (blocksUsed <= 4168)
    {
        // check to see if we need to allocate a new block
        if (offset > ((256 - size)))
        {
            //if the file has used the maximum blocks, return 0
            if (blocksUsed == 4168)
                return 0;

            void * newBlockAddr = findAndSetBlock();
            if (!newBlockAddr)
                return 0;

            /*
             We need to allocate a new block for the file. In this case, we need
             to look at the block of single indirect pointers, pointed to by the double
             indirect pointer. Find the last single indirect pointer, then find the last
             direct pointer in the block pointed to by the single indirect pointer.
             Set appropriate pointers and return the address of the new block for the file

            */
            unsigned int numBlocksPointedToByDIP = blocksUsed - 72;
            unsigned int numSIPs = (numBlocksPointedToByDIP / 64) + 1;
            unsigned int lastSIPindex = ((numSIPs - 1) * 4);

            unsigned int lastSIPblockAddress = *(unsigned int *)((unsigned int)inode->location[9] + lastSIPindex);

            unsigned int numFullSIPs = numBlocksPointedToByDIP / 64;
            unsigned int numDPsPointedToByLastSIP = (numBlocksPointedToByDIP - (64 * numFullSIPs));

            if (numDPsPointedToByLastSIP == 64)
            {
                unsigned int newSIPblockAddress = (unsigned int)findAndSetBlock();
                //adds a new SIP to the list of SIPs
                *((unsigned int *)((unsigned int)inode->location[9] + (lastSIPindex + 4))) = newSIPblockAddress;
                //sets first direct pointer to new block in the new SIP
                *(unsigned int *)newSIPblockAddress = (unsigned int) newBlockAddr;
            }
            else
            {
                unsigned int newDPaddr = *(unsigned int *)(lastSIPblockAddress + ((numDPsPointedToByLastSIP ) * 4));
                *(unsigned int *) newDPaddr = (unsigned int) newBlockAddr;
            }
            return newBlockAddr;

        }
        else
        {
            /*
                Need to return the address of the last block plus the offset. To get the
                address of the last block, we need to get the last single indirect pointer's
                value, then get the last direct pointer's value within the block pointed to
                by the single indirect pointer
            */
            unsigned int numBlocksPointedToByDIP = blocksUsed - 72;
            unsigned int numSIPs = (numBlocksPointedToByDIP / 64) + 1;
            unsigned int lastSIPindex = ((numSIPs - 1) * 4);

            unsigned int lastSIPblockAddress = *(unsigned int *)((unsigned int)inode->location[9] + lastSIPindex);

            unsigned int numFullSIPs = numBlocksPointedToByDIP / 64;
            unsigned int numDPsPointedToByLastSIP = (numBlocksPointedToByDIP - (64 * numFullSIPs));


            unsigned int lastDPaddr = *(unsigned int *)(lastSIPblockAddress + ((numDPsPointedToByLastSIP - 1) * 4));

            return (void *)(lastDPaddr + (unsigned int)offset);
        }
    }
    return 0;
}






/*
Takes in the directoryiNode (inUse, type, size, location[10], rights), the filename,
and the file's iNode number. This will have to append a (filename, fileiNodeNumber) entry
to the end of the directories file.
    - get address of where to insert directory entry

*/
int addDirectoryEntry(struct iNode * directoryiNode, char * filename, int fileiNodeNumber)
{

    //get the block to append to, and then offset into that block
    unsigned int blockToWriteToAddr = getBlockAddress(directoryiNode, (directoryiNode->numBlocks -1));
    if(!(blockToWriteToAddr))
        return 0;

    char * appendAddress = 0;
    //looks for an empry directory entry within the block
    for (int i = 0; i < 16; i++)
    {
        if(*((char*)(blockToWriteToAddr + (i * 16))) == 0)
        {
            appendAddress = ((char*)(blockToWriteToAddr + (i * 16)));
            break;
        }
    }
    if (!(appendAddress))
    {
        //TODO: (finished) There isn't space for a directory entry in the file's last block
        //TODO: (finished) allocate a new block and append to it there
        //TODO: allocateBlock() still doesn't update numBlocks properly
        appendAddress = (char *)allocateBlock(directoryiNode);
        if (!(appendAddress))
        {
            terminal_writestring("Couldn't enter new directory entry");
            return 0;
        }
    }
    //have to first insert the 14 bytes corresponding to the filename to appendAddress
    int flag = 0;
    for (int i = 0; i < 14; i++)
    {
        if (*filename == '\0')
            flag = 1;
        if (!flag)
            *appendAddress = *filename;
        else
            *appendAddress = '\0';
        filename++;
        appendAddress++;
    }
    *((short *)appendAddress) = (short)fileiNodeNumber;

    return 1;
}




void initializeRAMDisk()
{
    //terminal_writestring("In initializeRAMDisk()    ");
    void * locationCounter = (void *)startOfRAMdisk;

    *(int *)locationCounter = totalBlocks - 1;
    locationCounter += 4;
    *(int *)locationCounter = totalINodes -1;
    locationCounter += 4;


    for (int i = 0; i < 248; i++)
    {
    *(char *)locationCounter = (char) 0;
    locationCounter++;
    }


    //fill in the first index node with details about the root directory
    //locationCounter is equal to start of first index node
    struct iNode * rootInode = locationCounter;
    rootInode->inUse = 1;
    rootInode->type = 0;
    rootInode->size = 0;
    rootInode->location[0] = (void *) startFiles;
    rootInode->rights = 0;
    rootInode->usedBy = 0;
    rootInode->numBlocks = 1;

    locationCounter += 64;


    //fill in the rest of  256 blocks with 0s after the first index node
    for (int i = 0; i < 65472; i++)
    {
        *(char *)locationCounter = (char) 0;
        locationCounter++;
    }

    //set initial bit in bitmap to 1 to signify that the first block is being used by the root directory
    *(unsigned char*)locationCounter = (unsigned char) 0x01;
    locationCounter++;

    //fill in the next 4 blocks with 0s
    for (int i = 0; i < 1023; i++)
    {
        *(char *)locationCounter = (char) 0;
        locationCounter++;
    }

    //terminal_writestring("initialized bitmap");


    for (int i = 0; i < 2030336; i++)
    {
        *(char *)locationCounter = (char) 0;
        locationCounter++;
    }


    //terminal_writestring("initialized blocks");
    locationCounter = (void *) startFDTs;
    int numBytesInFDT = (MAX_FILES_IN_FDT * 8) * MAX_THREADS;
    for (int i = 0; i < numBytesInFDT; i++)
    {
        *(char *)locationCounter = (char) 0;
        locationCounter++;
    }

    return;
}





int deleteDirEntry(struct iNode * dir, char * dirEntry)
{

    int rootFlag = 0;
    if(dir == ((struct iNode *)startINodeArray))
        rootFlag = 1;
    char * blockStart = (char *)((unsigned int)dirEntry - ((unsigned int)dirEntry % 256));

    //start of block containing directory entries
    char * blockS = blockStart;

    //sets dirEntry to 0s
    for (int i = 0; i < 16; i++)
    {
        *(dirEntry) = 0;
        dirEntry++;
    }

   int count = 0;
   for (int i = 0; i < 16; i++)
   {
        if(*blockStart == 0)
        {
            count++;
        }
        else{
            break;
        }
        blockStart = (char *)((unsigned int) blockStart + 16);
   }

   if (count == 16)
   {
        //need to f
        if (dir->numBlocks <= 8)
        {
            int index = -1;
            if (dir->numBlocks == 1)
                return 0;

            for (int i = 0; i < 8; i++)
            {
                if (dir->location[i] == (void *)blockS){
                    index = i;
                    break;
                }
            }
            if (index >= 0)
            {

                for (int i = index; i < 7 ; i++)
                {
                    if (rootFlag && (i == 0 ))
                    {
                        char * baseBlock = (char *)startFiles;
                        char * tempP = (char *)dir->location[1];
                        for (int k = 0; k < 256; k++)
                        {
                            *baseBlock = *tempP;
                            baseBlock++;
                            tempP++;
                        }
                        findAndSetBlockToNull((unsigned int)dir->location[1]);
                        dir->numBlocks--;
                    }
                    else
                    {
                        dir->location[i] = dir->location[i+1];
                    }
                }
                if (!(rootFlag && (index == 0 )))
                {
                    findAndSetBlockToNull((unsigned int)blockS);
                    dir->numBlocks--;
                }
            }

        }
        else if (dir->numBlocks <= 72)
        {
            int index = -1;
            for (int i = 0; i < 8; i++)
            {
                if (dir->location[i] == (void *)blockS){
                    index = i;
                    break;
                }
            }
            if (index >= 0)
            {

                for (int i = index; i < 7 ; i++)
                {
                    dir->location[i] = dir->location[i+1];

                }
                if (rootFlag && (index == 0 ))
                {
                    char * baseBlock = (char *)0x610500;
                    char * tempP = (char *)dir->location[0];
                    for (int i = 0; i < 256; i++)
                    {
                        *baseBlock = *tempP;
                        baseBlock++;
                        tempP++;
                    }
                    findAndSetBlockToNull((unsigned int)dir->location[0]);
                    dir->location[0] = (void *)0x610500;
                }
            }
            else
            {
                //start of SIP block
                unsigned int * SIPp = (unsigned int *)dir->location[8];
                //count of empty pointers
                //loop through all the direct pointers
                for (int i = 0; i < 64; i++)
                {
                    if (*SIPp == (unsigned int)blockS)
                    {
                        *SIPp = 0;
                    }
                    SIPp = (unsigned int *)((unsigned int)SIPp + 4);
                }
                if (checkFreeSIPBlock(SIPp))
                {
                    findAndSetBlockToNull((unsigned int)SIPp);
                    dir->location[8] = (void *)0;
                }
            }
        }

        /*
        else
        {
            int index = 0;
            for (int i = 0; i < 8; i++)
            {
                if (dir->location[i] == (void *)blockS)
                {
                    index = i;
                    break;
                }
            }
            for (int i = 7; i > index; i--)
            {
                dir->location[i-1] = dir->location[i];
                dir->location[i] = (void *) 0;
            }

            //start of SIP block
            unsigned int * SIPp = (unsigned int *)dir->location[8];
            //count of empty pointers
            //loop through all the direct pointers
            for (int i = 0; i < 64; i++)
            {
                if (*SIPp == (unsigned int)blockS)
                {
                    *SIPp = 0;
                }
                SIPp = (unsigned int *)((unsigned int)SIPp + 4);
            }
            if (checkFreeSIPBlock(SIPp))
            {
                findAndSetBlockToNull((unsigned int)SIPp);
                dir->location[8] = (void *)0;
            }


            //point to start of DIP block
            unsigned int ** DIPp = (unsigned int **)dir->location[9];

            int count = 0;
            for (int i = 0; i < 64; i++)
            {
                unsigned int * sipP = *DIPp;
                for (int x = 0; x < 64; x++)
                {
                    if (*sipP == (unsigned int)blockS)
                    {
                        *sipP = 0;
                    }
                    sipP = (unsigned int *)((unsigned int)SIPp + 4);
                }
                if(checkFreeSIPBlock(sipP))
                {
                    findAndSetBlockToNull((unsigned int)sipP);
                    count++;
                }
                DIPp = ((unsigned int **)((unsigned int )DIPp + 4));
            }

            if (count == 64)
            {
                findAndSetBlockToNull((unsigned int)DIPp);
                dir->location[9] = (void *)0;
            }

        }
        */
   }
    return 0;

}






int rd_unlink(char * pathname){

    //gets the directory iNode
    struct iNode * diriNode = getNewFileDirectoryInode(pathname);

    if(diriNode == 0){
        terminal_writestring("Invalid Pathname");
        return -1;
    }

    //filename to be unlinked
    char * filename = (pathname + getDirectoriesStrLength(pathname));

    unsigned int dirEntryAddr = checkDirectoryEntries(diriNode, filename);
    if(!dirEntryAddr)
    {
        //terminal_writestring("File does not exist in parent directory");
        return -1;
    }

    //gets the iNodeNumber
    short fileiNodeNumber = *((short *)(dirEntryAddr + 14));
    if (fileiNodeNumber == 0){
        terminal_writestring("You cannot unlink the root directory");
        return -1;
    }

    //iNode of file we're unlinking
    struct iNode * fileiNode = ((struct iNode *)calculateInodeAddress(fileiNodeNumber));
    // Checks if the file is open
    if ( fileiNode->usedBy > 0){
        terminal_writestring("Cannot unlink; file in use");
        return -1;
    }
    if (fileiNode->type == 0 && fileiNode->size != 0){
        terminal_writestring("Cannot unlink a non-empty directory");
        return -1;
    }
    int nBlocks = fileiNode->numBlocks;
    for (int i = 0; i < nBlocks; i++)
    {
        unsigned int blockAddr = getBlockAddress(fileiNode, i);
        if (!blockAddr)
        {
            terminal_writestring("Couldn't find block address corresponding to block number");
            return -1;
        }
        findAndSetBlockToNull(blockAddr);
    }
    if(nBlocks>8)
        findAndSetBlockToNull((unsigned int)fileiNode->location[8]);
    if(nBlocks> 72 )
    {
        unsigned int DIP = (unsigned int)(fileiNode->location[9]);
        int numSIP = ((nBlocks - 72) / 64) + 1;

        for (int i = 0; i < numSIP; i++)
        {
            unsigned int sipP = *(unsigned int *)(DIP + (i * 4));
            findAndSetBlockToNull(sipP);

        }
        findAndSetBlockToNull(DIP);
    }
    // Zero out everything in the iNode
    fileiNode->inUse = 0;
    fileiNode->numBlocks = 0;
    fileiNode->size = 0;
    fileiNode->type = 0;
    fileiNode->usedBy = 0;
    fileiNode->rights = 0;
    for (int k = 0; k < 10; k++){
        fileiNode->location[k] = ((void *) 0);
    }
    sb->remainingInodes += 1;



    //deleteDirEntry(diriNode, (char *)dirEntryAddr);
    for (int i = 0; i < 16; i++)
    {

      *(char *)dirEntryAddr = (char)0;
    }




    //increment amount of available iNodes in the SuperBlock
    //sb->remainingBlocks += nBlocks;
    return 0;

}













//TODO: 1
//TODO:
//TODO:
//TODO:
//TODO:
//TODO add the if statements to check if there was an error
/**************************************************************
Creates a file
1.) Make sure that superblock is updated properly (See all functions that need to update it)
**************************************************************/
int rd_create(char *pathname, int mode)
{
    if (sb->remainingBlocks == 0 || sb->remainingInodes == 0)
    {
        terminal_writestring("Failed remaining resources check. Returning -1");
        return -1;
    }


    //holds the address of the iNode corresponding to the directory file for
    //the file we're creating
    //test1 always returns root directory
    //TODO: (finished) there is a function call to checkDirectoryEntries that needs to be checked
    struct iNode * directoryiNode = getNewFileDirectoryInode(pathname);
    if (!directoryiNode)
        return -1;

    //points to the start of the filename within the pathname
    char * filename = (pathname + getDirectoriesStrLength(pathname));

    //function call creates an iNode for the file, meaning it also allocates one block to the file,
    //and returns the iNode index (number)
    //TODO: (finished) check findAndSetiNode
    int fileiNodeNumber = findAndSetiNode(1, mode);
    if (fileiNodeNumber < 0)
        return -1;

    //terminal_writestring("iNode index: ");
    //terminal_writeint(fileiNodeNumber);
    //if that file already exists within the directory, return -1
    /****this works******/
    if (checkDirectoryEntries(directoryiNode, filename))
    {
        terminal_writestring("checkDirectoryEntries() found matching entry, returning -1");
        return -1;
    }

    //TODO: check addDirectoryEntry
    //TODO: addDirectoryEntry uses appendToAddress
    if (!(addDirectoryEntry(directoryiNode, filename, fileiNodeNumber))) {
        terminal_writestring("addDirectoryEntry() returning -1");
        return -1;
    }

/*
    terminal_writestring("Created file : ");
    terminal_writestring(filename);
    terminal_writestring("    With pathname: ");
    terminal_writestring(pathname);

    terminal_writestring("     With inode Number: ");
    terminal_writeint(fileiNodeNumber);
*/
    return 0;
}


int rd_mkdir(char *pathname)
{
    //check superblock
    if (sb->remainingBlocks == 0 || sb->remainingInodes == 0)
    {
        terminal_writestring("Failed remaining resources check. Returning -1");
        return -1;
    }

    //holds the address of the iNode corresponding to the directory file for
    //the file we're creating
    struct iNode * parentDirectoryiNode = getNewFileDirectoryInode(pathname);

    //points to the start of the filename within the pathname
    char * newDirName = (pathname + getDirectoriesStrLength(pathname));

    //function call creates an iNode for the file, meaning it also allocates one block to the file,
    //and returns the iNode index (number). The 1 stands for 'dir' type, 0 stands for read-write mode
    int fileiNodeNumber = findAndSetiNode(1, 0);

    //if that file already exists within the directory, return -1
    if (checkDirectoryEntries(parentDirectoryiNode, newDirName))
    {
        terminal_writestring("checkDirectoryEntries() found matching entry, returning -1");
        return -1;
    }

    if (!(addDirectoryEntry(parentDirectoryiNode, newDirName, fileiNodeNumber)))
     {
        terminal_writestring("addDirectoryEntry() returning -1");
        return -1;
    }



    terminal_writestring("Created Directory : ");
    terminal_writestring(newDirName);
    terminal_writestring("    With pathname: ");
    terminal_writestring(pathname);

    terminal_writestring("     With inode Number: ");
    char lcStr[10];
    itoa(lcStr, 'd', fileiNodeNumber);
    terminal_writestring(lcStr);



    return 0;

}





/*

    It checks if the flags used to open the file are allowed by the file's
    access rights. Gets the address of the iNode corresponding to the file that
    is being opened. Creates a new file object and inserts it into the calling
    thread's FDT. Returns index into the FDT where the file object is inserted

    //TODO: Synchronization

    @param - struct File_Object * FDT: Points to the first File_Object in the FDT

*/
int rd_open(char *pathname, int flags, struct File_Object * FDT){


     struct iNode * directoryiNode = getNewFileDirectoryInode(pathname);


     char * filename = (pathname + getDirectoriesStrLength(pathname));
     unsigned int dirEntryAddr = checkDirectoryEntries(directoryiNode, filename);//This function should return the iNode corresponding
                                                                                //to the file being opened
     short iNodeNum = *(short *)(dirEntryAddr + 14);
     struct iNode * fileiNode = (struct iNode *)calculateInodeAddress(iNodeNum);


    if (!fileiNode)
    {
        terminal_writestring("The file you are trying to open does not exist.");
        return -1;
    }
    //Checking if thread is allowed to open this file
    if(!(fileiNode->rights == 0 || fileiNode->rights==flags)){
        terminal_writestring("This file does not have the specified rights");
        return -1;
    }

    /*
    if(fileiNode->current_rights ==2){
        terminal_writestring("File in use");
        //Possibly implement semaphores here where process waits for file?
        return -1;
    }
    else if (fileiNode->current_rights == 1 && flags !=1){
        terminal_writestring("Cannot open file with this privilige, it is being read by someone else");
        return -1;
    }
    */


    //Need to find next free memory location in FDT
    bool flag = 0;
    int i = 0;
    for(; i<MAX_FILES_IN_FDT; i++){

        if(FDT->ptr == 0)
        {
            flag = 1;
            break;
        }
        FDT = (struct File_Object *)((unsigned int)FDT + 8);
    }

    if(!flag){
        terminal_writestring("No Space in FDT for new File Object");
        return -1;
    }

    //if(flags == 1) fileiNode->current_rights = 1;
    //else fileiNode->current_rights = 2;
    FDT->pos = 0;
    FDT->ptr = fileiNode;
    fileiNode->usedBy++;

    return i;
}



  int rd_lseek(int fd, int offset, struct File_Object * FDT){
    FDT = (struct File_Object *) ((unsigned int) FDT + (fd*8));
    if(FDT->ptr == 0){
        terminal_writestring("No File Object for that fd value");
        return -1;
    }
    struct iNode * fileiNode= FDT->ptr;
    if (fileiNode->type==0){
        terminal_writestring("You cannot seek from a directory");
        return -1;
    }
    int size = fileiNode->size;     //get the size of the file
    if (FDT->pos + offset > size){
        FDT->pos = size;
        return 0;
    }
    FDT->pos = FDT->pos + offset;
    return 0;
  }


int rd_chmod(char * pathname, int flag){
    struct iNode * diriNode = getNewFileDirectoryInode(pathname);
    if(diriNode == 0){
        terminal_writestring("You cannot change the rights for a non-existent file");
        return -1;
    }
    char * filename = (pathname + getDirectoriesStrLength(pathname));
    short fileiNodeNumber = *((short *)(checkDirectoryEntries(diriNode, filename) + 14));
    struct iNode * fileiNode = (struct iNode *)calculateInodeAddress(fileiNodeNumber);
    if ( fileiNode->usedBy > 0){
        terminal_writestring("Cannot change mode of a file that is in use");
        return -1;
    }
    if (fileiNode->type ==0 ){
        terminal_writestring("Cannot change mode of a directory");
        return -1;
    }
    fileiNode->rights = flag;
    return 0;
}





int readFromDirectBlock(char * address, int bytes, char * block, int pos){
    int counter = pos;
    block = (char *)((unsigned int)block+pos);
    while (counter< 256 && bytes>0){
        *address = *block;
        block = (char *) ((unsigned int) block +1);
        address++;
        counter++;
        bytes--;
    }
    return counter;
}

int rd_read(int fd, char * address, int num_bytes, struct File_Object * FDT, char * buffer){
    FDT = (struct File_Object *) ((unsigned int) FDT + (fd*8));
    if(FDT->ptr == 0){
        terminal_writestring("No File Object for that fd value");
        return -1;
    }
    struct iNode * fileiNode= FDT->ptr;
    if (fileiNode->type==0){
        terminal_writestring("You cannot read data from a directory");
        return -1;
    }
    int size = fileiNode->size;     //get the size of the file
    size -= FDT->pos;               //get the remaining bytes left to read from file (given current position)
    int current_block = FDT->pos/256; // calculate which block we need to start looking from
    int position_in_block = FDT->pos - current_block*256; //Stores position of cursor inside current block
    int total_blocks = fileiNode->numBlocks;
    if (num_bytes>size) num_bytes = size; //making sure we don't try to read more bytes than exist
    int total_bytes_read = num_bytes;       //return Value is stored in here
    int bytes_to_read = 256-position_in_block;
    while(current_block < total_blocks && num_bytes>0){
        int bytes_read = readFromDirectBlock(address, bytes_to_read, (char *)getBlockAddress(fileiNode,current_block), position_in_block);
        num_bytes -= bytes_read;
        FDT->pos += bytes_read;
        if(num_bytes>256) bytes_to_read = 256;
        else bytes_to_read = num_bytes;
        current_block++;
        position_in_block = 0;
    }
    return total_bytes_read;
}








/*
    Used fd to index into the thread's FDT and clears the file object at
    index fd. Returns -1 if that file object didn't exist.

    TODO: Synchronization

*/
int rd_close(int fd, struct File_Object * FDE){

    FDE = (struct File_Object *)((unsigned int)FDE + (fd * 8));
    if (FDE->ptr==0){
        terminal_writestring("No File Object for that fd value");
        return -1;
    }



    //    struct iNode * fileiNode = FDT->ptr;
    //fileiNode->inUse = 0;
    FDE->pos = 0;
    FDE->ptr = 0;
    struct iNode * inode = *((struct iNode **)((unsigned int)FDE + 4));
    inode->usedBy--;
    return 0;
}



//TODO: update iNode correctly
int rd_write(int fd, char *address, int num_bytes, struct File_Object * FDT)
{
    //gets the address of the file object entry in the FDT
    struct File_Object * FDE = (struct File_Object *)((unsigned int)FDT + (fd * 8));

    //gets the write position
    int writePos = *((int *)FDE);
    //gets the address of the inode of the file to be written to
    struct iNode * inode = *((struct iNode **)((unsigned int)FDE + 4));

    int blockToWriteTo = (writePos / 256);
    int byteOffset = (writePos % 256);

    char * writeToAddr = (char *)(Sbuffer + writePos);

    /*
        Now we have the address to write to
        - need to make sure that we don't write past the block limit (use the byteOffset)
        - if we get to the end of a block, allocate a new block and start writing there
    */

    int blockOffset = byteOffset;

    int i = 0;
    for(; i < num_bytes; i++)
    {
        // if offset equals 256, block (overflow)
        if (blockOffset == 256)
        {
            blockToWriteTo++;
            writeToAddr = (char *)(getBlockAddress(inode, blockToWriteTo));

            //check to see if we need to allocate a new block for file
            if (writeToAddr == 0)
            {
                writeToAddr = (char *)(allocateBlock(inode));
                if (!(writeToAddr))
                {
                    terminal_writestring("Couldn't allocate new block for write");
                    return i+1;
                }
            }
            blockOffset = 0;
        }


        //TODO: Update size appropriately
        //writes the character referenced to by 'address' to the file
        *writeToAddr = *address;

        //point to next character to write to
        address++;
        writeToAddr++;
        blockOffset++;
    }

    *((int *)FDE) = writePos + i;
    inode->size =  writePos + i;

    return i+1;

}


void memset(char * data, char c, int size)
{
    for (int i = 0; i < size; i++)
    {
        *data = c;
        data++;
    }
    return;
}

//append the filname and num to pathname
void sprintf(char * pname, char * c, int num)
{
    int cLen = strLen(c);
    for (int i = 0; i < cLen; i++)
    {
        *pname = *c;
        pname++;
        c++;
    }

    char intStr[10];
    itoa(intStr, 'd', num);
    int intStrLen = strLen(intStr);

    char * temp = intStr;
    for (int x = 0; x < intStrLen; x++)
    {
        *pname = *temp;
        pname++;
        temp++;
    }
    *pname = '\0';

    return;
}

int main(int argc, char * argv[])
{
    //terminal_writestring("in Main");
    initializeRAMDisk();


    //terminal_writestring("Returned From initializeRAMDisk()");

    //initializeThreads();

    int retval, i;
    int fd;
    int index_node_number;

    /* Some arbitrary data for our files */
    memset (data1, '1', sizeof (data1));
    memset (data2, '2', sizeof (data2));
    memset (data3, '3', sizeof (data3));

    //sprintf (pathname, "/file", 10);
    //terminal_writestring(pathname);



    #ifdef TEST1

      // ****TEST 1: MAXIMUM file creation****

      // Generate MAXIMUM regular files
    //for (i = 0; i < MAX_FILES; i++) {
    for (i = 0; i < MAX_FILES; i++) {
        sprintf (pathname, "/file", i);

        retval = CREAT (pathname, RD);

        if (retval < 0)
        {
            //fprintf (stderr, "creat: File creation error! status: %d (%s)\n", retval, pathname);
            terminal_writestring("     creat: File creation error! status: ");
            terminal_writeint(retval);
            terminal_writestring(" (");
            terminal_writestring(pathname);
            terminal_writestring(")      ");
            terminal_writestring("   iteration: ");
            terminal_writeint(i);

            //perror("Error!");

            if (i != MAX_FILES)
                return -1;
        }

        memset (pathname, 0, 80);
    }
    terminal_writestring("Passed First Part of Test 1 without any errors");


      // Delete all the files created
      //for (i = 0; i < MAX_FILES; i++) {
      for (i = 0; i < MAX_FILES; i++) {
        sprintf (pathname, "/file", i);

        //terminal_writestring("   This is the pathname: ");
        //terminal_writestring(pathname);

        retval = UNLINK (pathname);

        if (retval < 0) {
          terminal_writestring("unlink: File deletion error! status: ");
          terminal_writeint(retval);
          terminal_writestring("   iteration: ");
          terminal_writeint(i);
          return -1;
        }

        memset (pathname, 0, 80);
      }

    terminal_writestring("Passed all of test1 without any errors");


    #endif // TEST1


    #ifdef TEST2

    /* ****TEST 2: LARGEST file size**** */


    /* Generate one LARGEST file */
    retval = CREAT ("/bigfile", RW);

    if (retval < 0) {
        //fprintf (stderr, "creat: File creation error! status: %d\n",
        //     retval);

        terminal_writestring("creat: File creation error! status: ");
        terminal_writeint(retval);

        //exit(EXIT_FAILURE);
        return -1;
    }



    retval =  OPEN ("/bigfile", READWRITE, (struct File_Object *)startFDTs);
    /* Open file to write to it */

    if (retval < 0) {
        //fprintf (stderr, "open: File open error! status: %d\n",
        //     retval);

        terminal_writestring("open: File open error! status: ");
        terminal_writeint(retval);

        //exit(EXIT_FAILURE);
        return -1;
    }

    fd = retval;			/* Assign valid fd */

    /* Try writing to all direct data blocks */
    retval = WRITE (fd, data1, sizeof(data1), (struct File_Object *)startFDTs);

    if (retval < 0) {
        //fprintf (stderr, "write: File write STAGE1 error! status: %d\n",
        //     retval);
        terminal_writestring("write: File write STAGE1 error! status: ");
        terminal_writeint(retval);

        //exit(EXIT_FAILURE);
        return -1;
    }

    #ifdef TEST_SINGLE_INDIRECT

      /* Try writing to all single-indirect data blocks */
      retval = WRITE (fd, data2, sizeof(data2), (struct File_Object *)startFDTs);

      if (retval < 0) {
        //fprintf (stderr, "write: File write STAGE2 error! status: %d\n",
        //     retval);
        terminal_writestring("write: File write STAGE2 error! status: ");
        terminal_writeint(retval);

        //exit(EXIT_FAILURE);
        return -1;
      }

    #ifdef TEST_DOUBLE_INDIRECT

      /* Try writing to all double-indirect data blocks */
      retval = WRITE (fd, data3, sizeof(data3), (struct File_Object *)startFDTs);

      if (retval < 0) {
        //fprintf (stderr, "write: File write STAGE3 error! status: %d\n",
        //     retval);
        terminal_writestring("write: File write STAGE3 error! status: ");
        terminal_writeint(retval);

        //exit(EXIT_FAILURE);
        return -1;
      }

    #endif // TEST_DOUBLE_INDIRECT

    #endif // TEST_SINGLE_INDIRECT

    #endif // TEST2

    terminal_writestring("     Passed test 2     ");





    #ifdef TEST3

  /* ****TEST 3: Seek and Read file test**** */
  retval = LSEEK (fd, 0, (struct File_Object *)startFDTs );	/* Go back to the beginning of your file */

  if (retval < 0) {
    //fprintf (stderr, "lseek: File seek error! status: %d\n",
	     //retval);
	 terminal_writestring("lseek: File seek error! status: ");
	 terminal_writeint(retval);

    //exit(EXIT_FAILURE);
    return -1;
  }

  /* Try reading from all direct data blocks */
  retval = READ (fd, addr, sizeof(data1), (struct File_Object *)startFDTs, (char *) Sbuffer);

  if (retval < 0) {
    //fprintf (stderr, "read: File read STAGE1 error! status: %d\n",
	  //   retval);
	  terminal_writestring("read: File read STAGE1 error! status: ");
	  terminal_writeint(retval);

    //exit(EXIT_FAILURE);
    return -1;
  }
  /* Should be all 1s here... */
  //printf ("Data at addr: %s\n", addr);
  terminal_writestring("Data at addr: ");
  terminal_writestring((char *)(Sbuffer+retval));
  //printf ("Press a key to continue\n");
  //getchar(); // Wait for keypress

#ifdef TEST_SINGLE_INDIRECT

  /* Try reading from all single-indirect data blocks */
  retval = READ (fd, addr, sizeof(data2), (struct File_Object *)startFDTs, (char *) Sbuffer);

  if (retval < 0) {
    //fprintf (stderr, "read: File read STAGE2 error! status: %d\n",
	  //   retval);

    terminal_writestring("read: File read STAGE2 error! status: ");
    terminal_writeint(retval);
    //exit(EXIT_FAILURE);
    return -1;
  }
  /* Should be all 2s here... */
  //printf ("Data at addr: %s\n", addr);
  terminal_writestring("Data at addr: ");
  terminal_writestring((char *)(Sbuffer+retval));
  //printf ("Press a key to continue\n");
  //getchar(); // Wait for keypress

#ifdef TEST_DOUBLE_INDIRECT

  /* Try reading from all double-indirect data blocks */
  retval = READ (fd, addr, sizeof(data3), (struct File_Object *)startFDTs, (char *) Sbuffer);

  if (retval < 0) {
    //fprintf (stderr, "read: File read STAGE3 error! status: %d\n",
	  //   retval);
    terminal_writestring("read: File read STAGE3 error! status: ");
    terminal_writeint(retval);
    //exit(EXIT_FAILURE);
    return -1;
  }
  /* Should be all 3s here... */
  //printf ("Data at addr: %s\n", addr);
  //printf ("Press a key to continue\n");
  //getchar(); // Wait for keypress
  terminal_writestring("Data at addr: ");
  terminal_writestring((char *)(Sbuffer+retval));


#endif // TEST_DOUBLE_INDIRECT

#endif // TEST_SINGLE_INDIRECT

  /* Close the bigfile */
  retval = CLOSE(fd, (struct File_Object *)startFDTs);

  if (retval < 0) {
    //fprintf (stderr, "close: File close error! status: %d\n",
	  //   retval);
    terminal_writestring("read: File read STAGE2 error! status: ");
    terminal_writeint(retval);

    //exit(EXIT_FAILURE);
    return -1;
  }

#endif // TEST3


#ifdef TEST4

 
  retval = CHMOD("bigfile", RD); // Change bigfile to read-only
  
  if (retval < 0) {
    //fprintf (stderr, "chmod: Failed to change mode! status: %d\n",
      // retval);

    //exit(EXIT_FAILURE);
    return -1;
  }

  
  retval = WRITE (fd, data1, sizeof(data1),(struct File_Object *)startFDTs);
  if (retval < 0) {
    //fprintf (stderr, "chmod: Tried to write to read-only file!\n");
    //printf ("Press a key to continue\n");
    //getchar(); // Wait for keypress
    return -1;
  }
  
  

  retval = UNLINK ( "/bigfile");
  
  if (retval < 0) {
    //fprintf (stderr, "unlink: /bigfile file deletion error! status: %d\n",
      // retval);
    
    //exit(EXIT_FAILURE);
    return -1;
  }

#endif // TEST4
terminal_writestring("Passed Test 4 with no erros");

*/

  #ifdef TEST5
  
  /* ****TEST 5: Make directory including entries **** */
  retval = MKDIR ("/dir1");
    
  if (retval < 0) {
    //fprintf (stderr, "mkdir: Directory 1 creation error! status: %d\n",
      // retval);

    //exit(EXIT_FAILURE);
    return -1;
  }

  retval = MKDIR ("/dir1/dir2");
    
  if (retval < 0) {
    //fprintf (stderr, "mkdir: Directory 2 creation error! status: %d\n",
      // retval);

    //exit(EXIT_FAILURE);
     return -1;
  }

  retval = MKDIR ("/dir1/dir3");
    
  if (retval < 0) {
    //fprintf (stderr, "mkdir: Directory 3 creation error! status: %d\n",
      // retval);

    //exit(EXIT_FAILURE);
     return -1;
  }

#endif // TEST5










    /*
    initializeThreads();

    //assigns threads a task, sets busy flag, adds them to queue
    int threadStackStart = 0x110000;
    for (int i = 0; i < MAX_THREADS; i++)
    {
      thread_create(threadFunction, (void *) threadStackStart);
      threadStackStart += 0x100;
    }


    schedule();
    */

    return 0;
}




//TODO: should we append '/' to directory names within directory entries?
//TODO: fix functions using size to determine the number of blocks
//TODO: make sure numBlocks is updated correctly
//TODO: check when a file object isn't empty, but it points to a file that has been deleted
//TODO: need to make sure that we free blocks after findAndSetBlock() if things fail
    //  - implement freeBlock()
//TODO: check initialize Blocks in initializeRAMDisk()