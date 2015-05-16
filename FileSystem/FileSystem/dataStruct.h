#include "define.h"
struct supblock
{
	//unsigned int blockNum;				//the number of the block
	unsigned int size;				//the size of the disk
	unsigned int freeBlock[BLOCKNUM];	//the stack of the free block
	unsigned int	nextFreeBlock;		//the pointer of the next free block in the stack
	unsigned int freeBlockNum;			//the totally number of the free block in the disk
	unsigned int freeInode[INODENUM];	//the stack of the free node
	unsigned int freeInodeNum;			//the totally number of the free inode in the disk
	unsigned int nextFreeInode;		//the next free inode in the stack
	unsigned int lastLogin;
	/**注意：
	*如果有新的元素添加，请追加，不要插在数据之间
	**/
};

//node structure in the disk
struct finode
{
	int			mode;
	long		int	fileSize;
	int			fileLink;
	char			owner[MAXNAME];
	char			group[GROUPNAME];
	long		int	modifyTime;
	long		int	createTime;
	int			addr[6];
	char			black[45];				//凑整，也防止以后数据结构中又有新的东西加入影响程序
};

//node structure in the memory
struct inode
{
	struct					finode finode;
	struct					inode *parent;
	unsigned short int		inodeID;				//the node id
	int						userCount;			//the number of process using the inode
};
//direct structure
struct direct
{
	char					directName[DIRECTNAME];
	unsigned short int	inodeID;
};
//the structure of dir
struct dir
{
	int		dirNum;
	struct	direct direct[DIRNUM];
};
//the structure of file
struct file
{
	struct	inode *fInode;         
    int		f_curpos; 
};
//the structure of user
struct user
{
	char		userName[MAXNAME];
	char		userPwd[MAXPWD];
	char		userGroup[MAXNAME];
};