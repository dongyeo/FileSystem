#define INODENUM		20
#define BLOCKNUM		20
#define MAXNAME		20
#define MAXPWD		20
#define DIRECTNAME	14
#define GROUPNAME		15
#define DIRNUM		63
#define BOOTPOS		1024
#define SUPERSIZE		1024
#define INODESIZE		931840				//the number of inode in the disk is 7280 ,so the inodesize is 7280*128 and the disk is ((7280*128)+(2+7280)*1024)/(1024*1024)8MB
#define INODE			128					//the size of each inode in the disk
#define BLOCKSTART	912					//the start pos of block
#define BLOCKSNUM		7280
#define BLOCKSIZE		1024