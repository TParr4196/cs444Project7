#include "image.h"
#include "block.h"
#include "free.h"
#include "inode.c"
#include "ctest.h"

#ifdef CTEST_ENABLE

#define BLOCK_SIZE 4096

void test_image_open(void){
    CTEST_ASSERT(image_open("image.txt",0)>=0, "image_open returns file descriptor");
}

void test_image_close(void){
    CTEST_ASSERT(image_close()==0, "image_close close() success");
}

void test_bread_bwrite(void){
    unsigned char block_map[BLOCK_SIZE]={1,1,1,1,1,1};
    bwrite(2,block_map);
    unsigned char buffer[BLOCK_SIZE]={0};
    CTEST_ASSERT(*bread(2,buffer)==*block_map, "bread fills buffer with successful write");
}

void test_alloc(void){
    unsigned char block[BLOCK_SIZE]={0};
    for(int i=0; i<BLOCK_SIZE; i++){
        block[i]=0xff;
    }
    bwrite(2, block);
    CTEST_ASSERT(alloc()==-1, "returns -1 if block map is full");
    set_free(block, 4, 0);
    set_free(block, 6, 0);
    set_free(block, 15, 0);
    set_free(block, 24, 0);
    bwrite(2, block);
    int chk1 = alloc()==4;
    int chk2 = alloc()==6;
    int chk3 = alloc()==15;
    int chk4 = alloc()==24;
    CTEST_ASSERT(chk1&&chk2&&chk3&&chk4, "alloc arbitrary empty values verified");
}

void test_set_free(void){
    unsigned char block[16]={0};
    set_free(block, 3, 1);
    set_free(block, 17, 1);
    set_free(block, 13, 0);
    set_free(block, 42, 1);
    unsigned char byte=block[0];
    int chk1 = byte & (1 << 3);
    byte=block[2];
    int chk2 = byte & (1 << 1);
    byte=block[1];
    int chk3 = !(byte & (1 << 5));
    byte=block[5];
    int chk4 = byte & (1 << 2);
    CTEST_ASSERT(chk1&&chk2&&chk3&&chk4, "set_free arbitrary sets verified");
}

void test_find_free(void){
    unsigned char block[BLOCK_SIZE]={0};
    for(int i=0; i<BLOCK_SIZE; i++){
        block[i]=0xff;
    }
    CTEST_ASSERT(find_free(block)==-1, "find_free returns -1 if block is full");
    set_free(block, 2, 0);
    set_free(block, 5, 0);
    set_free(block, 12, 0);
    set_free(block, 22, 0);
    int chk1 = find_free(block)==2;
    set_free(block, 2, 1);
    int chk2 = find_free(block)==5;
    set_free(block, 5, 1);
    int chk3 = find_free(block)==12;
    set_free(block, 12, 1);
    int chk4 = find_free(block)==22;
    CTEST_ASSERT(chk1&&chk2&&chk3&&chk4, "find_free arbitrary empty values verified");
}

//check if two inodes have the same values
int check_inode(struct inode *a, struct inode *b){
    //check all unsigned ints, shorts, and chars
    int ck1 = a->size == b->size && a->owner_id == b->owner_id &&
        a->permissions == b->permissions && a->flags == b->flags && 
        a->link_count == b->link_count && a->ref_count == b->ref_count && a->inode_num == b->inode_num;

    //check block_ptr
    for(int i = 0; i < INODE_PTR_COUNT; i++){
        if(a->block_ptr[i] != b->block_ptr[i]){
            return 0;
        }
    }

    return ck1;
}

void test_ialloc(void){
    unsigned char block[BLOCK_SIZE]={0};
    for(int i=0; i<BLOCK_SIZE; i++){
        block[i]=0xff;
    }
    bwrite(1, block);
    CTEST_ASSERT(ialloc()==NULL, "returns -1 if inode map is full");
    set_free(block, 30000, 0);
    set_free(block, 30500, 0);
    set_free(block, 31000, 0);
    set_free(block, 32000, 0);
    bwrite(1, block);
    struct inode *in;
    in=ialloc();
    int chk1 = in->inode_num==30000;
    in=ialloc();
    int chk2 = in->inode_num==30500;
    in=ialloc();
    int chk3 = in->inode_num==31000;
    in=ialloc();
    int chk4 = in->inode_num==32000;
    struct inode test={0,0,0,0,0,{0},0,0};
    test.inode_num=32000;
    int chk5 = check_inode(&test, in);
    CTEST_ASSERT(chk1&&chk2&&chk3&&chk4&&chk5, "ialloc arbitrary empty values verified");
}

void test_incore_find_free(void){
    struct inode *test_inode=incore_find_free();
    CTEST_ASSERT(incore_find_free()==test_inode, "returns struct inode pointer");
    test_inode->ref_count=1;
    CTEST_ASSERT(incore_find_free()!=test_inode, "does not return pointer to in use inode");
    incore_all_used();
    CTEST_ASSERT(incore_find_free()==NULL, "returns NULL when no free incore inodes remain");
    incore_free_all();
    CTEST_ASSERT(incore_find_free()==test_inode, "all_used and free_all working");
}

void test_incore_find(void){
    struct inode *test_inode=incore_find_free();
    test_inode->inode_num=10;
    CTEST_ASSERT(incore_find(10)==test_inode, "finds a given inode_num");
    test_inode->ref_count=1;
    CTEST_ASSERT(incore_find(10)==NULL, "returns NULL if inode_num in use");
    test_inode=incore_find_free();
    test_inode->inode_num=10;
    CTEST_ASSERT(incore_find(10)==test_inode, "returns free inode_num");
    incore_all_used();
    CTEST_ASSERT(incore_find(10)==NULL, "returns NULL if all in use");
    incore_free_all();
    test_inode=incore_find_free();
    CTEST_ASSERT(incore_find(10)==test_inode, "returns first inode_num when refreed");
}

//I wanted to test them independently, but I couldn't think of a way to do that other than
//effectively rewriting the write_inode code for an individual inode in the test code
void test_read_write_inode(void){
    unsigned char block[BLOCK_SIZE]={0};
    bwrite(INODE_FIRST_BLOCK, block);
    struct inode in = {0,0,0,0,0,{0},0,0};
    struct inode test = {0,0,0,0,0,{0},0,0};
    read_inode(&test, 0);
    int ck1 = check_inode(&test, &in);
    in.size=1235231, in.flags='3', in.link_count=123;
    write_inode(&in);
    read_inode(&test,0);
    int ck2 = check_inode(&test, &in);
    unsigned short owner_id = (unsigned short) 12351235;
    unsigned short arbitrary = (unsigned short) 112451235;
    in.owner_id=owner_id, in.permissions='6', in.block_ptr[4]=arbitrary, in.inode_num=17;
    write_inode(&in);
    read_inode(&test, 17);
    int ck3 = check_inode(&test, &in);
    CTEST_ASSERT(ck1 && ck2 && ck3, "writes and reads arbitrary inodes successfully");
}

void test_iget(void){
    incore_all_used();
    CTEST_ASSERT(iget(0)==NULL, "returns NULL if all inodes are in use");
    incore_free_all();
    struct inode *in=iget(1244);
    CTEST_ASSERT(in!=NULL, "find an arbitrary new inode number");
    struct inode test = {0,0,0,0,0,{0},0,0};
    initialize_inode(in);
    test.ref_count=1;
    test.inode_num=in->inode_num;
    CTEST_ASSERT(check_inode(&test, iget(test.inode_num)), "retrive a previously found inode");
}

void test_iput(void){
    struct inode *in = iget(0);
    iput(in);
    CTEST_ASSERT(in->ref_count==0, "decrements ref_count to 0 if in use");
    iput(in);
    CTEST_ASSERT(in->ref_count==0, "decrements ref_count to 0 if not in use");
}

int main(){
    CTEST_VERBOSE(1);

    test_image_open();

    test_bread_bwrite();

    test_set_free();

    test_find_free();

    test_alloc();

    test_ialloc();

    test_incore_find_free();

    test_incore_find();

    test_read_write_inode();
    
    test_iget();

    test_iput();

    test_image_close(); //must be last

    CTEST_RESULTS();

    CTEST_EXIT();
}

#else

int main(){

    image_open("image.txt",0);

    //initialize an empty data block
    unsigned char block[4096]={0};
    for(int i=0; i<BLOCK_SIZE; i++){
        block[i]=0x0;
    }

    //not sure what goes in the superblock so giving it empty block
    bread(0,block); //superblock
    bread(1,block); //inode_map
    bread(3,block); //inode_data_block_0
    bread(4,block); //inode_data_block_1
    bread(5,block); //inode_data_block_2
    bread(6,block); //inode_data_block_3

    for(int i=0; i<7; i++){
        set_free(block, i,1);
    }

    bread(2,block); //block_map
    
    image_close();
}

#endif