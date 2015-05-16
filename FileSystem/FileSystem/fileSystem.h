#include	<iostream>
#include	<conio.h>
#include <fstream>
#include <time.h>
#include <stack>
#include <stdio.h>
#include <windows.h>
#include "dataStruct.h"
#include "error.h"

using namespace std;

//内存i节点数组，NUM为该文件系统容纳的文件数  
struct inode	usedinode[7280];  
//ROOT的内存i节点  
struct inode *root;
//当前节点
struct inode *current;
//已经打开文件的数组 
struct file* opened[];  
//超级块  
struct supblock *super; 
//模拟磁盘
FILE * virtualDisk;
//当前用户
struct user* curuser;
//当前文件或者目录名字（通过文件，获取名字和文件的inode id）
struct direct curdirect;
//当前目录
struct dir* curdir;
//退出
bool logout=false;
//用户文件节点
struct inode* userinode;
int userpos;

int syncinode(struct inode* inode)
{
	int ino;
	int ret;
	int ipos;
	ipos=BOOTPOS+SUPERSIZE+inode->inodeID*sizeof(struct finode);
	fseek(virtualDisk,ipos,SEEK_SET);
	ret=fwrite(&inode->finode,sizeof(struct finode),1,virtualDisk);
	fflush(virtualDisk);
	if(ret!=1)
		return ERROR_SYSINODE_FAIL;
	return ERROR_OK;
}

int initialize(char* path)
{
	virtualDisk=fopen(path,"r+");
	if(virtualDisk==NULL)
	{
		return ERROR_VM_NOEXIST;
	}
	super=(struct supblock*)calloc(1,sizeof(struct supblock));				//distribute space in the memory
	fseek(virtualDisk,BOOTPOS,SEEK_SET);								//the superblock is the #1	block
	fread(super,sizeof(struct supblock),1,virtualDisk);

	fseek(virtualDisk,BLOCKSTART*1024,SEEK_SET);
	//fread(super->freeBlock,sizeof(unsigned int),BLOCKNUM,virtualDisk);
	for(int i=0;i<20;i++)
		super->freeBlock[i]=912+i;
	super->nextFreeBlock=BLOCKNUM;
	super->size=8*1024*1024;
	super->nextFreeInode=INODENUM;
	for(int i=0;i<INODENUM;i++)
	{
		super->freeInode[i]=i;
	}
	super->freeBlockNum=BLOCKSNUM;
	super->freeBlockNum=BLOCKSNUM;

	/*for(int i=0;i<BLOCKNUM;i++)
		cout<<super->freeBlock[i]<<endl;
	cout<<super->nextFreeBlock;
	getchar();*/
	return ERROR_OK;
}
//format the virtual disk
int formatting(char * path)
{
	virtualDisk=fopen(path,"r+");
	if(virtualDisk==NULL)
	{
		return ERROR_VM_NOEXIST;
	}
	fseek(virtualDisk,BLOCKSTART*1024,SEEK_SET);
	unsigned int group[BLOCKNUM];
	for(int i=0;i<BLOCKNUM;i++)
	{
		group[i]=i+912;
	}
	for(int i=0;i<363;i++)
	{
		for(int j=0;j<BLOCKNUM;j++)
		{
			group[j]+=BLOCKNUM;
		}														
		fseek(virtualDisk,(BLOCKSTART+i*20)*1024,SEEK_SET);			//cout<<BLOCKSTART+i*20;
		fwrite(group,sizeof(unsigned int),BLOCKNUM,virtualDisk);
	}
	return ERROR_OK;
}
//save the super into the virtual disk
int synchronization()
{
	if(virtualDisk==NULL)
		return ERROR_VM_NOEXIST;
	fseek(virtualDisk,BOOTPOS,SEEK_SET);
	int writeSize=fwrite(super,sizeof(struct supblock),1,virtualDisk);
	fflush(virtualDisk);
	if(writeSize!=1)
		return ERROR_SYSSUP_FAIL;
	return ERROR_OK;
}
int loadSuper(char *path)
{
	virtualDisk=fopen(path,"r+");
	if(virtualDisk==NULL)
	{
		return ERROR_VM_NOEXIST;
	}
	super=(struct supblock*)calloc(1,sizeof(struct supblock));				//distribute space in the memory
	fseek(virtualDisk,BOOTPOS,SEEK_SET);								//the superblock is the #1	block
	int readSize=fread(super,sizeof(struct supblock),1,virtualDisk);
	if(readSize!=sizeof(struct supblock))
		return ERROR_LOADSUP_FAIL;
	return ERROR_OK;
}

struct inode * iget(int ino)
{
	int ipos	=	0;
	int ret	=	0;
	if(usedinode[ino].userCount!=0)
	{
		usedinode[ino].userCount++;
		return &usedinode[ino];
	}
	if(virtualDisk==NULL)
		return NULL;
	ipos=BOOTPOS+SUPERSIZE+ino*INODE;
	fseek(virtualDisk,ipos,SEEK_SET);
	ret=fread(&usedinode[ino],sizeof(struct finode),1,virtualDisk);
	if(ret!=1)
		return NULL;
	if(usedinode[ino].finode.fileLink==0)		//it is s new file
	{
		usedinode[ino].finode.fileLink++;
		usedinode[ino].finode.fileSize=0;
		usedinode[ino].inodeID=ino;
		time_t timer;
		time(&timer);
		usedinode[ino].finode.createTime=timer;
		super->freeInodeNum--;
		syncinode(&usedinode[ino]);
	}
	usedinode[ino].userCount++;
	usedinode[ino].inodeID=ino;
	return &usedinode[ino];
}
void iput(struct inode *inode)
{
	if(inode->userCount>0)
		inode->userCount--;
}

struct inode* ialloc()
{
	if(super->nextFreeInode==0)//if there is no free finode in the stack,get free node from the disk
	{
		int num=0;
		finode *tmp=(struct finode*)calloc(1,sizeof(struct finode));
		fseek(virtualDisk,BOOTPOS+SUPERSIZE,SEEK_SET);
		//cout<<"searching:"<<endl;
		for(int i=0;i<BLOCKSNUM;i++)
		{
			fread(tmp,sizeof(struct finode),1,virtualDisk);
			//cout<<"seek:"<<ftell(virtualDisk)<<endl;
			if(tmp->fileLink==0)
			{
				super->freeInode[num]=i;
				super->nextFreeInode++;
				num++;
			}
			if(num==20)
				break;
		}
	}
	if(super->nextFreeInode==0)
		return NULL;
	super->nextFreeInode--;
	synchronization();
	return iget(super->freeInode[super->nextFreeInode]);
}

int balloc()
{
	unsigned int bno;
	if(super->freeBlockNum<=0)
		return ERROR_BLOCK_EMPTY;
	if(super->nextFreeBlock==1)//get the next group of free block
	{
		bno=super->freeBlock[--super->nextFreeBlock];
		fseek(virtualDisk,super->freeBlock[0]*BLOCKSIZE,SEEK_SET);
		int ret=fread(super->freeBlock,sizeof(unsigned int),BLOCKNUM,virtualDisk);
		super->nextFreeBlock=ret;
		return bno;
	}
	super->freeBlockNum--;
	super->nextFreeBlock--;
	synchronization();
	return super->freeBlock[super->nextFreeBlock];
}
void getTime(long int timeStamp)
{
	time_t timer;
	timer=timeStamp;
	struct tm *p;
	p=gmtime(&timer);
	char s[80];
    strftime(s, 80, "%Y-%m-%d %H:%M:%S", p);
    printf("%s",s);
}
int bwrite(void * _Buf,unsigned short int bno,long int offset,int size,int count=1)
{
	long int pos;
	int ret;
	pos=bno*BLOCKSIZE+offset;
	fseek(virtualDisk,pos,SEEK_SET);
	ret=fwrite(_Buf,size,count,virtualDisk);
	fflush(virtualDisk);
	if(ret!=count)
		return ERROR_BWRITE_FAIL;
	return ERROR_OK;
}
int bread(void * _Buf,unsigned short int bno,int offset,int size,int count=1)
{
	long int pos;
	int ret;
	pos=bno*BLOCKSIZE+offset;
	fseek(virtualDisk,pos,SEEK_SET);
	ret=fread(_Buf,size,count,virtualDisk);
	if(ret!=count)
		return ERROR_BREAD_FAIL;
	return ERROR_OK;
}
int bfree(int bno)
{
	if(super->nextFreeBlock==20)
	{
		bwrite(&super->freeBlock,bno,0,sizeof(unsigned int),20);
		super->nextFreeBlock=1;
		super->freeBlock[0]=bno;
	}
	else
	{
		super->freeBlock[super->nextFreeBlock]=bno;
		super->nextFreeBlock++;
	}
	super->freeBlockNum++;
	synchronization();
	return 1;
}
void getMode(int mode)
{
	int type=mode/1000;
	int auth=mode%1000;
	if(type==1)
		cout<<"d";
	else
		cout<<"-";
	int div=100;
	for(int i=0;i<3;i++)
	{
		int num=auth/div;
		auth=auth%div;
		int a[3]={0};
		int time=2;
		while(num!=0)
		{
			a[time--]=num%2;
			num=num/2;
		}
		for(int i=0;i<3;i++)
		{
			if(a[i]==1)
			{
				if(i==2)
					cout<<"x";
				else if(i==1)
					cout<<"w";
				else if(i==0)
					cout<<"r";
			}
			else
				cout<<"-";
		}
		div/=10;
	}
}
void info(inode* inode)
{
	cout<<"mode:"<<inode->finode.mode<<endl;
	getMode(inode->finode.mode);
	cout<<endl;
	cout<<"owner:";
	cout.write(inode->finode.owner,MAXNAME);
	cout<<endl;
	cout<<"group:";
	cout.write(inode->finode.group,GROUPNAME);
	cout<<endl;
	getTime(inode->finode.createTime);
	cout<<endl;
	cout<<"FileLink:"<<inode->finode.fileLink<<endl;
	cout<<"fileSize:"<<inode->finode.fileSize<<endl;
	for(int i=0;i<6;i++)
			cout<<"addr"<<i<<":"<<inode->finode.addr[i]<<endl;
}
void superInfo()
{
	cout<<"size"<<super->size<<endl;
	cout<<"freeBlock:"<<endl;
	for(int i=0;i<super->nextFreeBlock;i++)
		cout<<super->freeBlock[i]<<" ";
	cout<<endl;
	cout<<"freeInode:"<<endl;
	for(int i=0;i<super->nextFreeInode;i++)
		cout<<super->freeInode[i]<<" ";
	cout<<"nextFreeInode:"<<super->nextFreeInode<<endl;
	cout<<"nextFreeBlock:"<<super->nextFreeBlock<<endl;
	cout<<"freeBlockNum:"<<super->freeBlockNum<<endl;
	cout<<"freeInodeNum:"<<super->freeInodeNum<<endl;
}
int login()
{
	char ch;
	struct dir* dir=(struct dir*)calloc(1,sizeof(struct dir));
	bread(dir,root->finode.addr[0],0,sizeof(struct dir));
	userinode=iget(dir->direct[0].inodeID);
	int usernum=userinode->finode.fileSize/sizeof(user);
	struct user* users=(struct user*)calloc(usernum,sizeof(struct user));
	bread(users,userinode->finode.addr[0],0,sizeof(struct user),usernum);
	char user[MAXNAME]={0},pwd[MAXPWD]={0};
	cout<<"username:";
	gets(user);
	for(int i=0;i<usernum;i++)
	{
		if(strcmp(users[i].userName,user)==0)
		{
			cout<<"password:";
			for(int i=0;i<MAXPWD;i++)
			{
				ch=getch();
				if(ch==13)
				{
					break;
				}
				pwd[i]=ch;
				cout<<" ";
			}
			cout<<endl;
			if(strcmp(users[i].userPwd,pwd)==0)
			{
				curuser=&users[i];
				userpos=i;
				time_t timer;
				time(&timer);
				cout<<"last login:";
				getTime(super->lastLogin);
				cout<<endl;
				super->lastLogin=timer+8*60*60;
				synchronization();
				return true;
			}
			else
			{
				cout<<"password is not match!"<<endl;
				return false;
			}
		}
	}
	cout<<"User does not exist"<<endl;
	return false;
}int strPos(char* str,int start,const char needle)
{
	for(int i=start;i<strlen(str);i++)
	{
		if(str[i]==needle)
			return i;
	}
	return -1;
}
int strCpy(char *dst,char *src,int offset)
{
	int len=strlen(src);
	if(len<=offset)
		return 0;
	int i;
	for(i=0;i<len-offset;i++)
	{
		dst[i]=src[i+offset];
		//cout<<"dst["<<i<<"]"<<dst[i]<<endl;
	}
	dst[i]=0;
	return 1;
}
int subStr(char* src,char*dst,int start,int end=-1)
{
	int pos=0;
	end==-1?end=strlen(src):NULL;
	for(int i=start;i<end;i++)
		dst[pos++]=src[i];
	dst[pos]=0;
	return 1;
}
int _cd(char* dir,inode* inode)
{
	return 0;
}
inode* cd(char* path,inode* inode)
{
	if(path[0]=='/'&&strlen(path)==1)
	{
		current=root;
		return NULL;
	}
	char tmp[16]={0};
	int start=path[0]=='/';
	int pos=strPos(path,1,'/');
	subStr(path,tmp,start,pos);
	int type=inode->finode.mode/1000;
	if(type==1)
	{
		int count=inode->finode.fileSize/sizeof(struct direct);
		int addrnum=count/63+(count%63>=1?1:0);
		dir * dir=(struct dir*)calloc(1,sizeof(struct dir));
		addrnum>4?addrnum=4:NULL;
		for(int addr=0;addr<addrnum;addr++)
		{
			bread(dir,inode->finode.addr[addr],0,sizeof(struct dir));
			for(int i=0;i<dir->dirNum;i++)
			{
				if(strcmp(dir->direct[i].directName,tmp)==0)
				{
					count=-1;
					curdirect=dir->direct[i];
					struct inode * tmpnode=iget(dir->direct[i].inodeID);
					tmpnode->parent=inode;
					inode=tmpnode;
					break;
				}
			}
			if(count==-1)
				break;
		}
		if(count!=-1)
			inode=NULL;
	}
	else
	{
		inode=NULL;
		return inode;
	}
	if(pos!=-1&&inode!=NULL)
	{
		subStr(path,tmp,pos+1);
		return cd(tmp,inode);
	}
	if(pos==-1)
		return inode;
}
int ls()
{
	int type=current->finode.mode/1000;
	if(type!=1)
	{
		cout<<"this is not a dir"<<endl;
		return 0;
	}
	int count=current->finode.fileSize/sizeof(struct direct);
	dir * dir=(struct dir*)calloc(1,sizeof(struct dir));
	int addrnum=count/63+(count%63>=1?1:0);
	addrnum>4?addrnum=4:NULL;
	for(int addr=0;addr<addrnum;addr++)
	{
		bread(dir,current->finode.addr[addr],0,sizeof(struct dir));
		for(int i=0;i<dir->dirNum;i++)
			puts(dir->direct[i].directName);
	}
	return 1;

}
int ll()
{
	int type=current->finode.mode/1000;
	if(type!=1)
	{
		cout<<"this is not a dir"<<endl;
		return 0;
	}
	int count=current->finode.fileSize/sizeof(struct direct);
	dir * dir=(struct dir*)calloc(1,sizeof(struct dir));
	int addrnum=count/63+(count%63>=1?1:0);
	addrnum>4?addrnum=4:NULL;
	for(int addr=0;addr<addrnum;addr++)
	{
		bread(dir,current->finode.addr[addr],0,sizeof(struct dir));
		for(int i=0;i<dir->dirNum;i++)
		{
			inode * inode=iget(dir->direct[i].inodeID);
			getMode(inode->finode.mode);
			cout<<" ";
			cout.write(inode->finode.owner,strlen(inode->finode.owner));
			cout<<" ";
			cout.write(inode->finode.group,strlen(inode->finode.group));
			cout<<" ";
			getTime(inode->finode.createTime);
			cout<<" ";
			puts(dir->direct[i].directName);
		}
	}
	return 1;

}
int mkdir(char * dirname)
{
	if(current->finode.mode/1000!=1)
	{
		cout<<"it is not a dir"<<endl;
		return -1;
	}
	int count=current->finode.fileSize/sizeof(struct direct);
	if(count>252)
	{
		cout<<"can not make more dir in the current dir!"<<endl;
		return -1;
	}
	dir * dir=(struct dir*)calloc(1,sizeof(struct dir));
	int addrnum=count/63+(count%63>=1?1:0);
	addrnum>4?addrnum=4:NULL;
	for(int addr=0;addr<addrnum;addr++)
	{
		bread(dir,current->finode.addr[addr],0,sizeof(struct dir));
		for(int i=0;i<dir->dirNum;i++)
			if(strcmp(dir->direct[i].directName,dirname)==0)
			{
				cout.write(dirname,strlen(dirname));
				cout<<" is exist in current dir!"<<endl;
				return -1;
			}
	}
	current->finode.fileSize+=sizeof(struct direct);
	syncinode(current);
	int addr=count/63;
	bread(dir,current->finode.addr[addr],0,sizeof(struct dir));
	strcpy(dir->direct[dir->dirNum].directName,dirname);
	struct inode * tmpinode=ialloc();
	tmpinode->finode.addr[0]=balloc();
	tmpinode->finode.mode=1774;
	strcpy(tmpinode->finode.owner,curuser->userName);
	strcpy(tmpinode->finode.group,curuser->userGroup);
	syncinode(tmpinode);
	dir->direct[dir->dirNum].inodeID=tmpinode->inodeID;
	dir->dirNum+=1;
	bwrite(dir,current->finode.addr[addr],0,sizeof(struct dir));
	return 1;
}
void cd__()
{
	int inodeid=current->inodeID;
	inode* inode=current->parent;
	int count=inode->finode.fileSize/sizeof(struct direct);
	dir * dir=(struct dir*)calloc(1,sizeof(struct dir));
	int addrnum=count/63+(count%63>=1?1:0);
	addrnum>4?addrnum=4:NULL;
	for(int addr=0;addr<addrnum;addr++)
	{
		bread(dir,inode->finode.addr[addr],0,sizeof(struct dir));
		for(int i=0;i<dir->dirNum;i++)
		{
			if(dir->direct[i].inodeID==inodeid)
			{
				count=-1;
				curdirect=dir->direct[i];
				break;
			}
		}
	}
}
void passwd()
{
	char ch;
	char passwd1[20]={0};
	char passwd2[20]={0};
	char passwd[20]={0};
	cout<<"please insert the old password:";
	for(int i=0;i<MAXPWD;i++)
	{
		ch=getch();
		if(ch==13)
		{
			break;
		}
		passwd[i]=ch;
		cout<<"*";
	}
	cout<<endl;
	if(strcmp(passwd,curuser->userPwd)==0)
	{
		cout<<"new password:";
		for(int i=0;i<MAXPWD;i++)
		{
			ch=getch();
			if(ch==13)
			{
				break;
			}
			passwd1[i]=ch;
			cout<<"*";
		}
		cout<<endl;
		cout<<"repeat the new password:";
		for(int i=0;i<MAXPWD;i++)
		{
			ch=getch();
			if(ch==13)
			{
				break;
			}
			passwd2[i]=ch;
			cout<<"*";
		}
		cout<<endl;
		if(strcmp(passwd1,passwd2)==0)
		{
			strcpy(curuser->userPwd,passwd1);
			bwrite(curuser,userinode->finode.addr[0],userpos*sizeof(struct user),sizeof(struct user));
		}
		else
		{
			cout<<"the password is not matched!"<<endl;
		}
	}
	else
		cout<<"the old password is not right!"<<endl;
	
}
int chmod(char * para)
{
	int mode=0;
	char dirname[100]={0};
	mode+=((para[0]-'0')>7?7:(para[0]-'0'))*100+((para[1]-'0')>7?7:(para[1]-'0'))*10+((para[2]-'0')>7?7:(para[2]-'0'));
	subStr(para,dirname,4);
	int count=current->finode.fileSize/sizeof(struct direct);
	dir * dir=(struct dir*)calloc(1,sizeof(struct dir));
	int addrnum=count/63+(count%63>=1?1:0);
	addrnum>4?addrnum=4:NULL;
	for(int addr=0;addr<addrnum;addr++)
	{
		bread(dir,current->finode.addr[addr],0,sizeof(struct dir));
		for(int i=0;i<dir->dirNum;i++)
		{
			if(strcmp(dir->direct[i].directName,dirname)==0)
			{
				inode* tmp=iget(dir->direct[i].inodeID);
				tmp->finode.mode=(tmp->finode.mode/1000)*1000+mode;
				syncinode(tmp);
				//info(tmp);
				return 1;
			}
		}
	}
	return -1;
}
int pwd()
{
	stack<char*> pwd;
	struct inode * inode=current;
	struct inode * parent=NULL;
	while(inode!=root)
	{
		int inodeid=inode->inodeID;
		parent=inode->parent;
		int count=parent->finode.fileSize/sizeof(struct direct);
		dir * dir=(struct dir*)calloc(1,sizeof(struct dir));
		int addrnum=count/63+(count%63>=1?1:0);
		addrnum>4?addrnum=4:NULL;
		for(int addr=0;addr<addrnum;addr++)
		{
			bread(dir,parent->finode.addr[addr],0,sizeof(struct dir));
			for(int i=0;i<dir->dirNum;i++)
			{
				if(dir->direct[i].inodeID==inodeid)
				{
					pwd.push(dir->direct[i].directName);
					count=-1;
					break;
				}
			}
			if(count==-1)
				break;
		}
		inode=inode->parent;
	}
	if(pwd.empty())
		cout<<"/";
	else
	{
		while(!pwd.empty())
		{
			cout<<"/";
			cout.write(pwd.top(),strlen(pwd.top()));
			pwd.pop();
		}
	}
	cout<<endl;
	return 1;
}
int mv(char* names)
{
	char oldname[16]={0},newname[16]={0};
	int pos=strPos(names,0,' ');
	subStr(names,oldname,0,pos);
	subStr(names,newname,pos+1);
	int count=current->finode.fileSize/sizeof(struct direct);
	dir * dir=(struct dir*)calloc(1,sizeof(struct dir));
	int addrnum=count/63+(count%63>=1?1:0);
	addrnum>4?addrnum=4:NULL;
	for(int addr=0;addr<addrnum;addr++)
	{
		bread(dir,current->finode.addr[addr],0,sizeof(struct dir));
		for(int i=0;i<dir->dirNum;i++)
		{
			if(strcmp(dir->direct[i].directName,oldname)==0)
			{
				strcpy(dir->direct[i].directName,newname);
				bwrite(dir,current->finode.addr[addr],0,sizeof(struct dir));
				return 1;
			}
		}
	}
	return 1;
}
int touch(char * filename)
{
	if(current->finode.mode/1000!=1)
	{
		cout<<"it is not a dir"<<endl;
		return -1;
	}
	int count=current->finode.fileSize/sizeof(struct direct);
	if(count>252)
	{
		cout<<"can not make more dir in the current dir!"<<endl;
		return -1;
	}
	dir * dir=(struct dir*)calloc(1,sizeof(struct dir));
	int addrnum=count/63+(count%63>=1?1:0);
	addrnum>4?addrnum=4:NULL;
	for(int addr=0;addr<addrnum;addr++)
	{
		bread(dir,current->finode.addr[addr],0,sizeof(struct dir));
		for(int i=0;i<dir->dirNum;i++)
			if(strcmp(dir->direct[i].directName,filename)==0)
			{
				cout.write(filename,strlen(filename));
				cout<<" is exist in current dir!"<<endl;
				return -1;
			}
	}
	current->finode.fileSize+=sizeof(struct direct);
	syncinode(current);
	int addr=count/63;
	bread(dir,current->finode.addr[addr],0,sizeof(struct dir));
	strcpy(dir->direct[dir->dirNum].directName,filename);
	struct inode * tmpinode=ialloc();
	tmpinode->finode.addr[0]=balloc();
	tmpinode->finode.mode=2774;
	strcpy(tmpinode->finode.owner,curuser->userName);
	strcpy(tmpinode->finode.group,curuser->userGroup);
	syncinode(tmpinode);
	dir->direct[dir->dirNum].inodeID=tmpinode->inodeID;
	dir->dirNum+=1;
	bwrite(dir,current->finode.addr[addr],0,sizeof(struct dir));
	return 1;
}
//int vi(char* filename)
//{
//	system("cls");
//	HANDLE hout;
//	COORD coord;
//	hout=GetStdHandle(STD_OUTPUT_HANDLE);
//	coord.X=0;
//	coord.Y=0;
//	SetConsoleCursorPosition(hout,coord);
//	int perLine=82;
//	char ch;
//	while(1)
//	{
//		ch=NULL;
//		ch=getch();
//		switch(int(ch))
//		{
//		case 72:			//up
//			if(coord.Y>0)
//				coord.Y-=1;
//			ch=0;
//			break;
//		case 80:			//down
//			coord.Y+=1;
//			ch=0;
//			break;
//		case 75:			//left
//			if(coord.X>0)
//				coord.X-=1;
//			else
//			{
//				if(coord.Y>0)
//				{
//					coord.X=perLine-1;
//					coord.Y-=1;
//				}
//			}
//			ch=0;
//			break;
//		case 77:			//right
//			if(coord.X<perLine-1)
//				coord.X+=1;
//			else
//			{
//				coord.X=0;
//				coord.Y+=1;
//			}
//			ch=0;
//			break;
//		}
//		if(ch==13)
//		{
//			coord.X=0;
//			coord.Y+=1;
//			cout<<endl;
//		}
//		else if(ch==8)
//		{
//			if(coord.X>0)
//				coord.X-=1;
//			else
//			{
//				if(coord.Y>0)
//				{
//					coord.X=perLine-1;
//					coord.Y-=1;
//				}
//			}
//			cout<<" ";
//		}
//		else if(ch>=32&&ch<=127)
//		{
//			cout<<ch;
//			coord.Y+=coord.X/(perLine-1);
//			coord.X=(coord.X==(perLine-1)?0:(coord.X+1));
//			ch=0;
//		}
//		SetConsoleCursorPosition(hout,coord);
//		/*if(ch==13)
//			cout<<endl;
//		else
//			cout<<ch;*/
//	}
//	return 1;
//}
int append(char * src)
{
	char filename[16]={0},content[8192]={0};
	int pos=strPos(src,0,' ');
	int inodeid=-1;
	subStr(src,filename,0,pos);
	subStr(src,content,pos+1);
	/*puts(filename);
	puts(content);
	cout<<strlen(content)<<endl;*/
	int count=current->finode.fileSize/sizeof(struct direct);
	dir * dir=(struct dir*)calloc(1,sizeof(struct dir));
	int addrnum=count/63+(count%63>=1?1:0);
	addrnum>4?addrnum=4:NULL;
	for(int addr=0;addr<addrnum;addr++)
	{
		bread(dir,current->finode.addr[addr],0,sizeof(struct dir));
		for(int i=0;i<dir->dirNum;i++)
		{
			if(strcmp(dir->direct[i].directName,filename)==0)
			{
				inodeid=dir->direct[i].inodeID;
				count=-1;
				break;
			}
		}
		if(count==-1)
			break;
	}
	if(inodeid==-1)
	{
		cout<<"can not found the file ";
		cout.write(filename,strlen(filename));
		cout<<endl;
		return -1;
	}
	struct inode * inode=iget(inodeid);
	int fileSize=inode->finode.fileSize;
	int index=0;
	int addr=fileSize/1024;
	int offset=fileSize%1024;
	int charCount=strlen(content)-index;
	int writeCount=(charCount>1024?1024-offset:charCount);
	bwrite(&content[index],inode->finode.addr[addr],offset,sizeof(char),writeCount);
	index+=writeCount;
	inode->finode.fileSize+=writeCount;
	while(index<strlen(content)&&addr<4)
	{
		inode->finode.addr[++addr]=balloc();
		offset=inode->finode.fileSize%1024;
		charCount=strlen(content)-index;
		writeCount=(charCount>1024?1024-offset:charCount);
		bwrite(&content[index],inode->finode.addr[addr],offset,sizeof(char),writeCount);
		inode->finode.fileSize+=writeCount;
	}
	syncinode(inode);
	return 1;
}
int cat(char * filename)
{
	int inodeid=0;
	int count=current->finode.fileSize/sizeof(struct direct);
	dir * dir=(struct dir*)calloc(1,sizeof(struct dir));
	int addrnum=count/63+(count%63>=1?1:0);
	addrnum>4?addrnum=4:NULL;
	for(int addr=0;addr<addrnum;addr++)
	{
		bread(dir,current->finode.addr[addr],0,sizeof(struct dir));
		for(int i=0;i<dir->dirNum;i++)
		{
			if(strcmp(dir->direct[i].directName,filename)==0)
			{
				inodeid=dir->direct[i].inodeID;
				count=-1;
				break;
			}
		}
		if(count==-1)
			break;
	}
	if(inodeid==0)
	{
		cout<<"can not found the file ";
		cout.write(filename,strlen(filename));
		cout<<endl;
		return -1;
	}
	struct inode* inode=iget(inodeid);
	int addr=inode->finode.fileSize/1024;
	int lastCount=inode->finode.fileSize%1024;
	int i;
	for(i=0;i<addr;i++)
	{
		char content[1024]={0};
		bread(&content,inode->finode.addr[i],0,sizeof(char),1024);
		cout.write(content,1024);
	}
	char content[1024]={0};
	bread(&content,inode->finode.addr[i],0,sizeof(char),lastCount);
	cout.write(content,strlen(content));
	cout<<endl;
	return 1;
}
int _rmdir(struct inode* inode)
{
	struct inode * rminode=NULL;
	int count=inode->finode.fileSize/sizeof(struct direct);
	dir * dir=(struct dir*)calloc(1,sizeof(struct dir));
	int addrnum=count/63+(count%63>=1?1:0);
	addrnum>4?addrnum=4:NULL;
	for(int addr=0;addr<addrnum;addr++)
	{
		bread(dir,inode->finode.addr[addr],0,sizeof(struct dir));
		for(int i=0;i<dir->dirNum;i++)
		{
			rminode=iget(dir->direct[i].inodeID);
			if(rminode->finode.mode/1000==1)
			{
				_rmdir(rminode);
			}
			int rmaddr=rminode->finode.fileSize/1024+(rminode->finode.fileSize%1024==0?0:1);
			if(rminode->finode.fileSize==0)
				rmaddr=1;
			for(int i=0;i<rmaddr;i++)
				bfree(rminode->finode.addr[i]);
			rminode->finode.fileLink=0;
			syncinode(rminode);
		}
	}
	return 1;
}
int rmdir(struct inode* inode,char* filename)
{
	struct inode* rminode=NULL;
	int count=inode->finode.fileSize/sizeof(struct direct);
	dir * dir=(struct dir*)calloc(1,sizeof(struct dir));
	int addrnum=count/63+(count%63>=1?1:0);
	addrnum>4?addrnum=4:NULL;
	for(int addr=0;addr<addrnum;addr++)
	{
		bread(dir,inode->finode.addr[addr],0,sizeof(struct dir));
		for(int i=0;i<dir->dirNum;i++)
		{
			if(strcmp(dir->direct[i].directName,filename)==0)
			{
				rminode=iget(dir->direct[i].inodeID);
				for(int j=i;j<dir->dirNum;j++)
					dir->direct[j]=dir->direct[j+1];
				dir->dirNum--;
				bwrite(dir,inode->finode.addr[addr],0,sizeof(struct dir));
				count=-1;
				break;
			}
		}
		if(count==-1)
			break;
	}
	_rmdir(rminode);
	int rmaddr=rminode->finode.fileSize/1024+(rminode->finode.fileSize%1024==0?0:1);
	if(rminode->finode.fileSize==0)
				rmaddr=1;
	for(int i=0;i<rmaddr;i++)
		bfree(rminode->finode.addr[i]);
	rminode->finode.fileLink=0;
	super->freeInodeNum++;
	synchronization();
	syncinode(rminode);
	return 1;
}
int chgrp(char *src)
{
	char filename[16]={0},groupname[GROUPNAME]={0};
	int pos=strPos(src,0,' ');
	int inodeid=-1;
	subStr(src,filename,0,pos);
	subStr(src,groupname,pos+1);
	//puts(filename);
	//puts(groupname);
	if(strcmp(curuser->userName,"root")!=0)
	{
		cout<<"Access deny!"<<endl;
		return -1;
	}
	int count=current->finode.fileSize/sizeof(struct direct);
	dir * dir=(struct dir*)calloc(1,sizeof(struct dir));
	int addrnum=count/63+(count%63>=1?1:0);
	addrnum>4?addrnum=4:NULL;
	for(int addr=0;addr<addrnum;addr++)
	{
		bread(dir,current->finode.addr[addr],0,sizeof(struct dir));
		for(int i=0;i<dir->dirNum;i++)
		{
			if(strcmp(dir->direct[i].directName,filename)==0)
			{
				inode * inode=iget(dir->direct[i].inodeID);
				strcpy(inode->finode.group,groupname);
				syncinode(inode);
				//bwrite(dir,current->finode.addr[addr],0,sizeof(struct dir));
				count=-1;
				return 1;
			}
		}
		if(count==-1)
			break;
	}
	cout<<"can not find file ";
	puts(filename);
	return 1;
}
int chown(char *src)
{
	char filename[16]={0},owner[MAXNAME]={0};
	int pos=strPos(src,0,' ');
	int inodeid=-1;
	subStr(src,filename,0,pos);
	subStr(src,owner,pos+1);
	//puts(filename);
	//puts(groupname);
	if(strcmp(curuser->userName,"root")!=0)
	{
		cout<<"Access deny!"<<endl;
		return -1;
	}
	int count=current->finode.fileSize/sizeof(struct direct);
	dir * dir=(struct dir*)calloc(1,sizeof(struct dir));
	int addrnum=count/63+(count%63>=1?1:0);
	addrnum>4?addrnum=4:NULL;
	for(int addr=0;addr<addrnum;addr++)
	{
		bread(dir,current->finode.addr[addr],0,sizeof(struct dir));
		for(int i=0;i<dir->dirNum;i++)
		{
			if(strcmp(dir->direct[i].directName,filename)==0)
			{
				inode * inode=iget(dir->direct[i].inodeID);
				strcpy(inode->finode.owner,owner);
				syncinode(inode);
				//bwrite(dir,current->finode.addr[addr],0,sizeof(struct dir));
				count=-1;
				return 1;
			}
		}
		if(count==-1)
			break;
	}
	cout<<"can not find file ";
	puts(filename);
	return 1;
}
int cp(char * src)
{
	char srcfile[16]={0},newfile[GROUPNAME]={0};
	int pos=strPos(src,0,' ');
	subStr(src,srcfile,0,pos);
	subStr(src,newfile,pos+1);
	int inodeid=0;
	int count=current->finode.fileSize/sizeof(struct direct);
	dir * dir=(struct dir*)calloc(1,sizeof(struct dir));
	int addrnum=count/63+(count%63>=1?1:0);
	addrnum>4?addrnum=4:NULL;
	for(int addr=0;addr<addrnum;addr++)
	{
		bread(dir,current->finode.addr[addr],0,sizeof(struct dir));
		for(int i=0;i<dir->dirNum;i++)
		{
			if(strcmp(dir->direct[i].directName,srcfile)==0)
			{
				inodeid=dir->direct[i].inodeID;
				count=-1;
				break;
			}
		}
		if(count==-1)
			break;
	}
	if(inodeid==0)
	{
		cout<<"can not found the file ";
		cout.write(srcfile,strlen(srcfile));
		cout<<endl;
		return -1;
	}
	inode* srcinode=iget(inodeid);
	count=current->finode.fileSize/sizeof(struct direct);
	if(count>252)
	{
		cout<<"can not make more dir in the current dir!"<<endl;
		return -1;
	}
	addrnum=count/63+(count%63>=1?1:0);
	addrnum>4?addrnum=4:NULL;
	for(int addr=0;addr<addrnum;addr++)
	{
		bread(dir,current->finode.addr[addr],0,sizeof(struct dir));
		for(int i=0;i<dir->dirNum;i++)
			if(strcmp(dir->direct[i].directName,newfile)==0)
			{
				cout.write(newfile,strlen(newfile));
				cout<<" is exist in current dir!"<<endl;
				return -1;
			}
	}
	current->finode.fileSize+=sizeof(struct direct);
	syncinode(current);
	int addr=count/63;
	bread(dir,current->finode.addr[addr],0,sizeof(struct dir));
	strcpy(dir->direct[dir->dirNum].directName,newfile);
	struct inode * tmpinode=ialloc();
	//addr distribut
	count=srcinode->finode.fileSize/1024;
	int srcaddr=srcinode->finode.fileSize/1024+(srcinode->finode.fileSize%1024==0?0:1);
	if(srcinode->finode.fileSize==0)
				srcaddr=1;
	for(int i=0;i<srcaddr;i++)
	{
		tmpinode->finode.addr[i]=balloc();
		char content[1024]={0};
		bread(&content,srcinode->finode.addr[i],0,sizeof(char),1024);
		bwrite(&content,tmpinode->finode.addr[i],0,sizeof(char),1024);
	}
	tmpinode->finode.fileSize=srcinode->finode.fileSize;
	tmpinode->finode.mode=srcinode->finode.mode;
	strcpy(tmpinode->finode.owner,srcinode->finode.owner);
	strcpy(tmpinode->finode.group,srcinode->finode.group);
	syncinode(tmpinode);
	dir->direct[dir->dirNum].inodeID=tmpinode->inodeID;
	dir->dirNum+=1;
	bwrite(dir,current->finode.addr[addr],0,sizeof(struct dir));
	return 1;
}
int dispatcher()
{
	char shell[8192]={0};
	char command[8192]={0};
	char ch;
	cout<<"["<<curuser->userName<<"@localhost "<<(current->inodeID==0?"/":curdirect.directName)<<"]"<<(strcmp(curuser->userName,"root")==0?"#":"$");
	ch=getchar();
	int i=0;
	if(ch==10)
		return 0;
	while(ch!=10)
	{
		shell[i]=ch;
		ch=getchar();
		i++;
	}
	strlwr(shell);
	strcpy(command,shell);
	strtok(command," ");
	if(strcmp(command,"ls")==0)
	{
		ls();
	}
	else if(strcmp(command,"ll")==0)
	{
		ll();
	}
	else if(strcmp(command,"mkdir")==0)
	{
		strCpy(command,shell,strlen(command)+1);
		mkdir(command);
	}
	else if(strcmp(command,"cd")==0)
	{
		inode* inode=NULL,*ret=NULL;
		strCpy(command,shell,strlen(command)+1);
		if(command[0]=='/')
			inode=root;
		else
			inode=current;
		ret=cd(command,inode);
		if(ret!=NULL)
			current=ret;
	}
	else if(strcmp(command,"cd../")==0)
	{
		if(current->parent!=NULL)
		{
			inode* tmp=current->parent;
			current=tmp;
			if(current->inodeID!=0)
				cd__();
		}
	}
	else if(strcmp(command,"passwd")==0)
	{
		strCpy(command,shell,strlen(command)+1);
		passwd();
	}
	else if(strcmp(command,"chmod")==0)
	{
		strCpy(command,shell,strlen(command)+1);
		if(chmod(command)==1)
		{
			cout<<"change succeed!"<<endl;
		}
		else
		{
			cout<<"direct not found!"<<endl;
		}
	}
	else if(strcmp(command,"mv")==0)
	{
		strCpy(command,shell,strlen(command)+1);
		mv(command);
	}
	else if(strcmp(command,"touch")==0)
	{
		strCpy(command,shell,strlen(command)+1);
		touch(command);
	}
	else if(strcmp(command,">>")==0)
	{
		strCpy(command,shell,strlen(command)+1);
		append(command);
	}
	else if(strcmp(command,"cat")==0)
	{
		strCpy(command,shell,strlen(command)+1);
		cat(command);
	}
	else if(strcmp(command,"rmdir")==0)
	{
		strCpy(command,shell,strlen(command)+1);
		rmdir(current,command);
	}
	else if(strcmp(command,"chgrp")==0)
	{
		strCpy(command,shell,strlen(command)+1);
		chgrp(command);
	}
	else if(strcmp(command,"chown")==0)
	{
		strCpy(command,shell,strlen(command)+1);
		chown(command);
	}
	else if(strcmp(command,"cp")==0)
	{
		strCpy(command,shell,strlen(command)+1);
		cp(command);
	}
	else if(strcmp(command,"pwd")==0)
		pwd();
	else if(strcmp(command,"info")==0)
		superInfo();
	else if(strcmp(command,"exit")==0)
		logout=true;
	//strCpy(command,shell,strlen(command)+1);
	return 1;
}