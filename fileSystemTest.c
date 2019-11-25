#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#define INODES_PER_BLOCK 16
#define SUPER_BLOCK_SIZE 1024
#define INODE_SIZE 64
#define DISK_BLOCK_SIZE 1024
#define SIZE_OF_FREE_ARRAY 150
#define SIZE_OF_FREE_INODE_ARRAY 100
#define Initial_Block_Start 21
#define SIZE_OF_ADDRESS 4

static FILE *diskfile;
static int nblocks=0;
static int ninodeblocks=0;
static char *filename1;
static char *prompt="fsys";
static char *end=">";
static int fd=0;
static unsigned long isize=0;
static struct fs_superblock super;
static struct fs_inode buffindoe;
static struct fs_inode rootInode;
static struct fs_inode currentInode;

struct fs_buf_freeblock{
	unsigned short buf_nfree;
	unsigned long buf_free[SIZE_OF_FREE_ARRAY];
};

struct fs_dir{
	short inode;
	char file_name[14];
};

struct fs_superblock {
	unsigned long isize;
	unsigned long fsize;
	unsigned short nfree;
	unsigned short ninode;
	short flock;
	short ilock;
	short fmod;
	unsigned short niblocks;
	unsigned short time[2];
	unsigned long free[SIZE_OF_FREE_ARRAY];
	unsigned long inode[SIZE_OF_FREE_INODE_ARRAY];
};

struct fs_inode {
	unsigned short flags;
	short nlinks;
	short uid;
	short gid;
	unsigned long size;
	unsigned long addr[10];
	unsigned short actime[2];
	unsigned short modtime[2];
	int used;
};

int initfs( const char *filename, int fsize,int ninodes )
{

	diskfile = fopen(filename,"r+");
	if(!diskfile) diskfile = fopen(filename,"w+");
	if(!diskfile) return 0;

	ninodeblocks=ninodes/INODES_PER_BLOCK;
	if(ninodes%INODES_PER_BLOCK)
	{ninodeblocks=ninodeblocks +1;}
	nblocks = fsize + ninodeblocks+2;
	ftruncate(fileno(diskfile),( nblocks*DISK_BLOCK_SIZE));
	filesetup(filename,fsize,ninodes);
    fclose(diskfile);
	return 1;
}

int disk_size()
{
	return nblocks;
}

int openfs(const char *filename)
{
	fd=open(filename,2);
	lseek(fd,DISK_BLOCK_SIZE,SEEK_SET);
	read(fd,&super,sizeof(super));
	lseek(fd,2*DISK_BLOCK_SIZE,SEEK_SET);
	read(fd,&rootInode,sizeof(rootInode));
	return 1;
}

void filesetup(const char *filename, int fsize,int ninodes)
{
	struct fs_inode inode[ninodes];
	super.fsize=fsize;
	super.ninode=0;
	super.niblocks=ninodeblocks;
	super.isize=ninodes;
	isize=ninodes;
	fd=open(filename,2);
	int i,j;
	////inodes other than root node
	for(i=1;i<ninodes;i++)
	{
        addInodetoSuper(i);
		inode[i].uid=i;
		inode[i].used=0;
		lseek(fd,2*DISK_BLOCK_SIZE+i*INODE_SIZE,SEEK_SET);
		write(fd,&inode[i].flags,sizeof(inode[i]));
	}

	////set all free blocks
	int h;
	for(h=0;h<fsize;h++)
    {
        addFreeBlock(2+ninodeblocks+h);
    }
	lseek(fd,DISK_BLOCK_SIZE,SEEK_SET);
	write(fd,&super.isize,sizeof(super));
	setRootInode();//set root inode and allocate a freeblock  for directory

}

/////can be ignored but stored for reference actuall call goes to addfree  function which is below it
int addFreeBlockold(unsigned long logicalblock)
{
    if(super.nfree==SIZE_OF_FREE_ARRAY)
    {
        printf("nfree is full \n");
        struct fs_buf_freeblock freebuf;
        printf("logical block to hold free is %d\n",logicalblock);
        freebuf.buf_nfree=super.nfree;
		int j;
        for(j=0;j<SIZE_OF_FREE_ARRAY;j++)
        {
            freebuf.buf_free[j]=super.free[j];
        }
        lseek(fd,0,0);
        lseek(fd,DISK_BLOCK_SIZE*logicalblock,0);
        write(fd,&freebuf.buf_nfree,sizeof(freebuf));
        super.nfree=1;
        super.free[0]=logicalblock;
    }
    else{
        super.free[super.nfree]=logicalblock;
        super.nfree++;
    }
    return 1;
}

int addFreeBlock(unsigned long logicalblock)
{
    ///to check currupted address blocks
    if(logicalblock>disk_size())
    {
        return 1;
    }
    if(super.nfree==SIZE_OF_FREE_ARRAY)////if super free is full
    {
        unsigned short arrayelementscount=SIZE_OF_FREE_ARRAY;
        unsigned long addressval=0;
        lseek(fd,0,0);
        lseek(fd,DISK_BLOCK_SIZE*logicalblock,SEEK_SET);
        write(fd,&arrayelementscount,sizeof(arrayelementscount));
		int j;
        for(j=0;j<SIZE_OF_FREE_ARRAY;j++)
        {
            addressval=super.free[j];
            lseek(fd,0,0);
            lseek(fd,DISK_BLOCK_SIZE*logicalblock+2+j*4,SEEK_SET);
            write(fd,&addressval,sizeof(addressval));
        }
        super.nfree=1;
        super.free[0]=logicalblock;
        return 1;
    }
    if(super.nfree<SIZE_OF_FREE_ARRAY){
        super.free[super.nfree]=logicalblock;
        super.nfree++;
        return 1;
    }
    //////this will not happen but  incasse
    if(super.nfree>SIZE_OF_FREE_ARRAY){
        super.nfree=SIZE_OF_FREE_ARRAY;
        return 1;
    }
    return 1;
}

unsigned long getFreeBlock()
{
    struct fs_buf_freeblock freebuf;
    super.nfree--;

    if(super.nfree==0)
    {
        unsigned long logicblock=0;
        logicblock=super.free[super.nfree];
        unsigned short arrayelementscount;
        unsigned long addressval=0;
        lseek(fd,0,0);
        lseek(fd,DISK_BLOCK_SIZE*logicblock,SEEK_SET);
        read(fd,&arrayelementscount,sizeof(arrayelementscount));
        super.nfree=arrayelementscount;
        if(super.nfree==0){return 0;}
		int i;
        for(i=0;i<SIZE_OF_FREE_ARRAY;i++)
        {
            lseek(fd,0,0);
            lseek(fd,DISK_BLOCK_SIZE*logicblock+2+i*4,SEEK_SET);
            read(fd,&addressval,sizeof(addressval));
            super.free[i]=addressval;
        }
        return logicblock;
    }
    else{
        return super.free[super.nfree];
    }
}

/////old functiio for reference the new fucntion is more accurate in rading check getfreeblock for new
unsigned long getFreeBlockold()
{
    struct fs_buf_freeblock freebuf;
    super.nfree--;

    if(super.nfree==0)
    {
        unsigned long logicblock=0;
        logicblock=super.free[super.nfree];
        lseek(fd,0,0);
        lseek(fd,DISK_BLOCK_SIZE*logicblock,SEEK_SET);
        read(fd,&freebuf.buf_free,sizeof(freebuf));
        super.nfree=freebuf.buf_free;
        if(super.nfree==0){return 0;}
		int i;
        for(i=0;i<SIZE_OF_FREE_ARRAY;i++)
        {
            super.free[i]=freebuf.buf_free[i];
        }
        return logicblock;
    }
    else{
        return super.free[super.nfree];
    }
}

///add free inode no to super block, so we can allocate new inode to new file,this fuction just puts the inode no in super
int addInodetoSuper(int inode)
{
	if(super.ninode<SIZE_OF_FREE_INODE_ARRAY)
	{
		super.inode[super.ninode]=inode;
		super.ninode++;
		return 1;
	}
	else
	{
		return 0;
	}
}

///set root inode with values
int setRootInode()
{
    struct fs_dir dir;
	rootInode.flags = 0140777;
	rootInode.nlinks = 1;
	rootInode.uid = 0;
	rootInode.gid = 0;
	rootInode.size = 0;
	rootInode.addr[0] =getFreeBlock();
	rootInode.addr[1] = 0;
	rootInode.modtime[0] = 0;
	rootInode.modtime[1] = 0;
	rootInode.used=1;
	lseek(fd, 2*DISK_BLOCK_SIZE,SEEK_SET);
	write(fd, &rootInode, INODE_SIZE);

	dir.inode = 0;
	strncpy(dir.file_name, ".", 14);
	lseek(fd, rootInode.addr[0]*DISK_BLOCK_SIZE, SEEK_SET);
	write(fd, &dir, 16);
	dir.inode = 0;
	strncpy(dir.file_name, "..", 14);
	write(fd, &dir, 16);
	int k;
	for(k=2;k<64;k++)///all other entries other than files are dir.inodeno=-1
	{
		dir.inode = -1;
		strncpy(dir.file_name, "....", 14);
		write(fd, &dir, 16);
	}
	currentInode=rootInode;
}

unsigned long getInode()
{
    if(super.ninode==0)
    {
        scanforInodes();
        return getInode();
    }
    else{
            super.ninode--;
        return super.inode[super.ninode];
    }
}

int scanforInodes()
{
    int isused;
    unsigned long offset= 2*DISK_BLOCK_SIZE;
	int k;
    for(k=0;k<isize;k++)
    {
        lseek(fd,offset+60+k*INODE_SIZE,SEEK_SET);
        read(fd,&isused,sizeof(isused));
        if(isused==0)
        {
            if(super.ninode<SIZE_OF_FREE_INODE_ARRAY)
            {
                super.inode[super.ninode]=k;
                super.ninode++;
            }
            else{
                break;
            }
        }
    }
}



int checkifFile(char *name)
{
	int i;
    for(i=0;i<strlen(name);i++)
        {
           if(name[i]=='.')
        {
        return 1;
            }
       }
       return 0;
}

int cpin(char *filename, char *v6filename)
{
	int extrnfilefd=open(filename,2);
	unsigned long fileSize =0;
	fileSize=lseek(extrnfilefd, 0, SEEK_END);
	//printf("filename:%s\n",token);
	//printf("inode:%d\n",hereinode.uid);
	return copyfile(fd,extrnfilefd,fileSize,v6filename);
	return 1;
}

int copyfile(int fd,int extrnfilefd,unsigned long size,char *v6filename)
{
    //struct fs_inode hereInode;
	unsigned long sizetransfered=0;
	int seek = lseek(extrnfilefd, 0, SEEK_SET);
	if(seek < 0 || size < 0){
		printf("Error copying file\n");
		return 0;
	}
	if(size>4294967296)
	{
		printf("file size exceeded!!!\n");
		return 0;
	}
	///get root directory to store inode and file name
	short inodeno =getInode();
	struct fs_dir dir;
	int file_count = 0;
	lseek(fd, (currentInode.addr[0]*DISK_BLOCK_SIZE), SEEK_SET);
	int count=0;
	read(fd, &dir, 16);
	while((dir.inode != -1) || (dir.inode != -2) && count<64){
            if(dir.inode == -1)
            {
                break;
            }
            if(dir.inode == -2)
            {
                break;
            }
            if(dir.inode<0)
            {
                break;
            }
		file_count++;
		read(fd, &dir, 16);
		count++;
	}

	dir.inode = inodeno;
	strncpy(dir.file_name, v6filename, 14);
	lseek(fd, (currentInode.addr[0]*DISK_BLOCK_SIZE)+(file_count*16), SEEK_SET);
	write(fd, &dir, 16);

    struct fs_inode inodebuff;
	int *buffer = (int *)malloc(DISK_BLOCK_SIZE);
    int totalblocksneeded=size/DISK_BLOCK_SIZE;
	inodebuff.flags=0100777;///for a file
	inodebuff.nlinks=0;
	inodebuff.uid=inodeno;
	inodebuff.gid=0;
	inodebuff.size=size;
	inodebuff.modtime[0]=0;
	inodebuff.modtime[1]=1;
	inodebuff.used=1;

	//directblock storing start
    int addresarrayno=0;
    while(addresarrayno<8 && size>sizetransfered)
    {
        unsigned long fblockno=getFreeBlock();
        if(size==sizetransfered)
        {
            break;
        }

		if((size-sizetransfered)>=DISK_BLOCK_SIZE)
		{
			//write content in to block number
			lseek(extrnfilefd,sizetransfered,SEEK_SET);
			read(extrnfilefd,buffer,DISK_BLOCK_SIZE);
			lseek(fd,fblockno*DISK_BLOCK_SIZE,SEEK_SET);
			write(fd,buffer,DISK_BLOCK_SIZE);
			sizetransfered=sizetransfered+DISK_BLOCK_SIZE;
		}
		else{
			//write content in to block number
			lseek(extrnfilefd,sizetransfered,SEEK_SET);
			read(extrnfilefd,buffer,size-sizetransfered);
			lseek(fd,fblockno*DISK_BLOCK_SIZE,SEEK_SET);
			write(fd,buffer,size-sizetransfered);
			sizetransfered=size;
		}
		inodebuff.addr[addresarrayno]=fblockno;
		addresarrayno++;
    }

	//directblock storing ends
	// single directblock storing start
	if(size>sizetransfered)
    {
	unsigned long fblocksinglelinkno=getFreeBlock();
	int noOfBlocksSingle=0;
	unsigned long arrayofadrblock[DISK_BLOCK_SIZE/SIZE_OF_ADDRESS];
	while(size>sizetransfered && noOfBlocksSingle<DISK_BLOCK_SIZE/SIZE_OF_ADDRESS)
	{
		unsigned long fblockstage2=getFreeBlock();
		if((size-sizetransfered)>=DISK_BLOCK_SIZE)
		{
			//write content in to block number
			lseek(extrnfilefd,sizetransfered,SEEK_SET);
			read(extrnfilefd,buffer,DISK_BLOCK_SIZE);
			lseek(fd,fblockstage2*DISK_BLOCK_SIZE,SEEK_SET);
			write(fd,buffer,DISK_BLOCK_SIZE);
			sizetransfered=sizetransfered+DISK_BLOCK_SIZE;
		}
		else{
			//write content in to block number
			lseek(extrnfilefd,sizetransfered,SEEK_SET);
			read(extrnfilefd,buffer,size-sizetransfered);
			lseek(fd,fblockstage2*DISK_BLOCK_SIZE,SEEK_SET);
			write(fd,buffer,size-sizetransfered);
			sizetransfered=size;
		}
		arrayofadrblock[noOfBlocksSingle]=fblockstage2;
		noOfBlocksSingle++;
	}
	lseek(fd,DISK_BLOCK_SIZE*fblocksinglelinkno,SEEK_SET);
    read(fd,&arrayofadrblock,sizeof(arrayofadrblock));
	inodebuff.addr[8]=fblocksinglelinkno;
    }
	// single directblock storing ends
	////triple directblock storing start
    if(size>sizetransfered)
    {
	unsigned long fblocktriplelinkno=getFreeBlock();
	int noOfBlocksfirsttriple=0;
	unsigned long arrayofFirstTripleAdrblock[DISK_BLOCK_SIZE/SIZE_OF_ADDRESS];

	while((size>sizetransfered) &&noOfBlocksfirsttriple<DISK_BLOCK_SIZE/SIZE_OF_ADDRESS)
	{
		unsigned long fblocktripleSecondlinkno=getFreeBlock();
		int noOfBlocksSecondtriple=0;
		unsigned long arrayofSecondTripleAdrblock[DISK_BLOCK_SIZE/SIZE_OF_ADDRESS];
		while((size>sizetransfered)&&noOfBlocksSecondtriple<DISK_BLOCK_SIZE/SIZE_OF_ADDRESS)
		{
			unsigned long fblocktripleThirdlinkno=getFreeBlock();
			int noOfBlocksThirdtriple=0;
			unsigned long arrayofThirdTripleAdrblock[DISK_BLOCK_SIZE/SIZE_OF_ADDRESS];

			while((size>sizetransfered) &&noOfBlocksThirdtriple<DISK_BLOCK_SIZE/SIZE_OF_ADDRESS)
			{
				unsigned long fblockfinal=getFreeBlock();
				if((size-sizetransfered)>=DISK_BLOCK_SIZE)
				{
					//write content in to block number
					lseek(extrnfilefd,sizetransfered,SEEK_SET);
					read(extrnfilefd,buffer,DISK_BLOCK_SIZE);
					lseek(fd,fblockfinal*DISK_BLOCK_SIZE,SEEK_SET);
					write(fd,buffer,DISK_BLOCK_SIZE);
					sizetransfered=sizetransfered+DISK_BLOCK_SIZE;
				}
				else
				{
					//write content in to block number
					lseek(extrnfilefd,sizetransfered,SEEK_SET);
					read(extrnfilefd,buffer,size-sizetransfered);
					lseek(fd,fblockfinal*DISK_BLOCK_SIZE,SEEK_SET);
					write(fd,buffer,size-sizetransfered);
					sizetransfered=size;
				}
				arrayofThirdTripleAdrblock[noOfBlocksThirdtriple]=fblockfinal;
				noOfBlocksThirdtriple++;
			}

			lseek(fd,DISK_BLOCK_SIZE*fblocktripleThirdlinkno,SEEK_SET);
			read(fd,&arrayofThirdTripleAdrblock,sizeof(arrayofThirdTripleAdrblock));
			noOfBlocksSecondtriple++;
		}
		lseek(fd,DISK_BLOCK_SIZE*fblocktripleSecondlinkno,SEEK_SET);
		read(fd,&arrayofSecondTripleAdrblock,sizeof(arrayofSecondTripleAdrblock));
		noOfBlocksfirsttriple++;
	}
	lseek(fd,DISK_BLOCK_SIZE*fblocktriplelinkno,SEEK_SET);
	read(fd,&arrayofFirstTripleAdrblock,sizeof(arrayofFirstTripleAdrblock));
	inodebuff.addr[9]=fblocktriplelinkno;
    }

	//////triple block storing ends
	/////write inode in to disk
	lseek(fd,2*DISK_BLOCK_SIZE+inodeno*INODE_SIZE,SEEK_SET);
	write(fd,&inodebuff,sizeof(inodebuff));

	return 1;
}


int newdir(char *dirname)
{
	return makeDir(fd,dirname);
}

int makeDir(int fd,char *arg2)
{
	short inodeno =getInode();
	struct fs_dir dir;
	int file_count = 0;
	lseek(fd, (currentInode.addr[0]*DISK_BLOCK_SIZE), SEEK_SET);
	read(fd, &dir, 16);
	int count=0;
	while((dir.inode != -1 || dir.inode != -2) &&count<64){
            if((dir.inode==-1)||(dir.inode==-2))
            {
                break;
            }
		file_count++;
		read(fd, &dir, 16);
		count++;
	}
	//printf("file_count: %d\n",file_count);
	dir.inode = inodeno;
	strncpy(dir.file_name, arg2, 14);
	lseek(fd, (currentInode.addr[0]*DISK_BLOCK_SIZE)+(file_count*16), SEEK_SET);
	write(fd, &dir, 16);
    struct fs_inode inodebuff;
    ////creating dir inode
	inodebuff.flags=0140755;////for directory
	inodebuff.nlinks=0;
	inodebuff.uid=inodeno;
	inodebuff.gid=0;
	inodebuff.size=0;
	inodebuff.modtime[0]=0;
	inodebuff.modtime[1]=1;
	inodebuff.used=1;
	inodebuff.addr[0]=getFreeBlock();
	lseek(fd,2*DISK_BLOCK_SIZE+inodeno*INODE_SIZE,SEEK_SET);
	write(fd,&inodebuff,sizeof(inodebuff));
	////modifying the data block of directory
	dir.inode = currentInode.uid;
	strncpy(dir.file_name, ".", 14);
	lseek(fd, inodebuff.addr[0]*DISK_BLOCK_SIZE, SEEK_SET);
	write(fd, &dir, 16);
	dir.inode = inodebuff.uid;
	strncpy(dir.file_name, "..", 14);
	write(fd, &dir, 16);
	int k;
	for(k=2;k<64;k++)
	{
		dir.inode = -1;
		strncpy(dir.file_name, "....", 14);
		write(fd, &dir, 16);
	}
}


int cpout(const char *v6filename, char *extfile)
{
	return copyout(fd,v6filename,extfile);
}

int copyout(int fd, char *v6filename, char *extfile){

	int file_count = 0;
	int flag = 0;
	struct fs_dir dir;
	struct fs_inode inode;
	int inodeNo;
	void *buffer = (void *)malloc(DISK_BLOCK_SIZE);
	int count=0;
	lseek(fd, (currentInode.addr[0]*DISK_BLOCK_SIZE), 0);
	read(fd, &dir, 16);
	while((dir.inode !=-1)&&count<64){
            if((dir.inode==-1))
            {
               break;
            }
		if(strcmp(v6filename, dir.file_name)==0){
			inodeNo = dir.inode;
			flag = 1;
			break;
		}
		file_count++;
		read(fd, &dir, 16);
		count++;
	}
	if(flag != 1){
		printf("\n$$ ERROR !!! SOURCE FILE DOES NOT EXISTS $$\n");
		return 1;
	}
	else
	{
		lseek(fd, 2*DISK_BLOCK_SIZE+inodeNo*INODE_SIZE,SEEK_SET);
		read(fd, &inode, sizeof(inode));
		printf("\n## CREATING NEW EXTERNAL FILE\n");

		unsigned long size=inode.size;
		createfile(extfile,size);
		int extfd=open(extfile,2);
		unsigned long sizetransfered=0;

		int addresarrayno=0;
		while(addresarrayno<8 && size>sizetransfered)
		{
			unsigned long fblockno=inode.addr[addresarrayno];

			if((size-sizetransfered)>=DISK_BLOCK_SIZE)
			{
				lseek(fd,fblockno*DISK_BLOCK_SIZE,SEEK_SET);
				read(fd,buffer,DISK_BLOCK_SIZE);
				//write content in to block number
				lseek(extfd,sizetransfered,SEEK_SET);
				write(extfd,buffer,DISK_BLOCK_SIZE);
				sizetransfered=sizetransfered+DISK_BLOCK_SIZE;
			}
			else{
				lseek(fd,fblockno*DISK_BLOCK_SIZE,SEEK_SET);
				read(fd,buffer,size-sizetransfered);
				//write content in to block number
				lseek(extfd,sizetransfered,SEEK_SET);
				write(extfd,buffer,size-sizetransfered);
				sizetransfered=size;
			}
			addresarrayno++;
		}

		if(size>sizetransfered)
		{
			unsigned long fblocksinglelinkno=inode.addr[8];
			int noOfBlocksSingle=0;
			unsigned long arrayofadrblock[DISK_BLOCK_SIZE/SIZE_OF_ADDRESS];
			lseek(fd,fblocksinglelinkno*DISK_BLOCK_SIZE,SEEK_SET);
			read(fd,arrayofadrblock,sizeof(arrayofadrblock));
			while(size>sizetransfered && noOfBlocksSingle<DISK_BLOCK_SIZE/SIZE_OF_ADDRESS)
			{
				unsigned long fblockstage2=arrayofadrblock[noOfBlocksSingle];
				if((size-sizetransfered)>=DISK_BLOCK_SIZE)
				{
					lseek(fd,fblockstage2*DISK_BLOCK_SIZE,SEEK_SET);
					read(fd,buffer,DISK_BLOCK_SIZE);
					//write content in to block number
					lseek(extfd,sizetransfered,SEEK_SET);
					write(extfd,buffer,DISK_BLOCK_SIZE);
					sizetransfered=sizetransfered+DISK_BLOCK_SIZE;
				}
				else{
					lseek(fd,fblockstage2*DISK_BLOCK_SIZE,SEEK_SET);
					read(fd,buffer,size-sizetransfered);
					//write content in to block number
					lseek(extfd,sizetransfered,SEEK_SET);
					write(extfd,buffer,size-sizetransfered);
					sizetransfered=size;
				}
				noOfBlocksSingle++;
			}
		}

		if(size>sizetransfered)
		{
			unsigned long fblocktriplelinkno=inode.addr[9];
			int noOfBlocksfirsttriple=0;
			unsigned long arrayofFirstTripleAdrblock[DISK_BLOCK_SIZE/SIZE_OF_ADDRESS];
			lseek(fd,fblocktriplelinkno*DISK_BLOCK_SIZE,SEEK_SET);
			read(fd,arrayofFirstTripleAdrblock,sizeof(arrayofFirstTripleAdrblock));
			while(size>sizetransfered &&noOfBlocksfirsttriple<DISK_BLOCK_SIZE/SIZE_OF_ADDRESS)
			{
				unsigned long fblocktripleSecondlinkno=arrayofFirstTripleAdrblock[noOfBlocksfirsttriple];
				int noOfBlocksSecondtriple=0;
				unsigned long arrayofSecondTripleAdrblock[DISK_BLOCK_SIZE/SIZE_OF_ADDRESS];
				lseek(fd,fblocktripleSecondlinkno*DISK_BLOCK_SIZE,SEEK_SET);
				read(fd,arrayofSecondTripleAdrblock,sizeof(arrayofSecondTripleAdrblock));
				while(size>sizetransfered &&noOfBlocksSecondtriple<DISK_BLOCK_SIZE/SIZE_OF_ADDRESS)
				{
					unsigned long fblocktripleThirdlinkno=arrayofSecondTripleAdrblock[noOfBlocksSecondtriple];
					int noOfBlocksThirdtriple=0;
					unsigned long arrayofThirdTripleAdrblock[DISK_BLOCK_SIZE/SIZE_OF_ADDRESS];
					lseek(fd,fblocktripleThirdlinkno*DISK_BLOCK_SIZE,SEEK_SET);
					read(fd,arrayofThirdTripleAdrblock,sizeof(arrayofThirdTripleAdrblock));
					while(size>sizetransfered &&noOfBlocksThirdtriple<DISK_BLOCK_SIZE/SIZE_OF_ADDRESS)
					{
						unsigned long fblockfinal=arrayofThirdTripleAdrblock[noOfBlocksThirdtriple];
						if((size-sizetransfered)>=DISK_BLOCK_SIZE)
						{
							lseek(fd,fblockfinal*DISK_BLOCK_SIZE,SEEK_SET);
							read(fd,buffer,DISK_BLOCK_SIZE);
							//write content in to block number
							lseek(extfd,sizetransfered,SEEK_SET);
							write(extfd,buffer,DISK_BLOCK_SIZE);
							sizetransfered=sizetransfered+DISK_BLOCK_SIZE;
						}
						else
						{
							lseek(fd,fblockfinal*DISK_BLOCK_SIZE,SEEK_SET);
							read(fd,buffer,size-sizetransfered);
							//write content in to block number
							lseek(extfd,sizetransfered,SEEK_SET);
							write(extfd,buffer,size-sizetransfered);
							sizetransfered=size;
						}
						noOfBlocksThirdtriple++;
					}
					noOfBlocksSecondtriple++;
				}
				noOfBlocksfirsttriple++;
			}
		}
	}
}


int createfile( const char *filename,unsigned long size)
{
	diskfile = fopen(filename,"r+");
	if(!diskfile) diskfile = fopen(filename,"w+");
	if(!diskfile) return 0;
	ftruncate(fileno(diskfile),size);
    fclose(diskfile);
	return 1;
}


int rm(const char *filename)
{
 return rmove(fd, filename);
}


int rmove(int fd, char *filename)
{
	int file_count = 0;
	int flag = 0;
	struct fs_dir dir;
	struct fs_inode inode;
	int inodeNo;
    unsigned long rmSize=0;
    unsigned long sizeRemoved=0;
	lseek(fd,(currentInode.addr[0]*DISK_BLOCK_SIZE), SEEK_SET);
	read(fd, &dir, 16);
	while(dir.inode != -1){
            if((strcmp(".",filename)==0)||(strcmp("..",filename)==0))
            {
                continue;
            }
		if(strcmp(filename, dir.file_name)==0){
			inodeNo = dir.inode;
			flag = 1;
			break;
		}
		file_count++;
		read(fd, &dir, 16);
	}

	if(flag != 1){
		printf("\n$$ ERROR!!!! SOURCE FILE DOES NOT EXISTS $$\n");
		return 1;
	}
	else
	{
		lseek(fd,2*DISK_BLOCK_SIZE+inodeNo*INODE_SIZE,SEEK_SET);
		read(fd,&inode,sizeof(inode));
		rmSize=inode.size;
		int addrblockno=0;
		while(addrblockno<8)
		{
		    ////for directory rmsize is 0 so to remove one block it should enter into if loop so rm=1;
		    if(rmSize==0)
            {
                rmSize=1;
            }
		    if(rmSize>sizeRemoved)
            {
            unsigned long adrno=0;
		    lseek(fd,2*DISK_BLOCK_SIZE+inodeNo*INODE_SIZE+12+addrblockno*4,SEEK_SET);
            read(fd,&adrno,sizeof(adrno));
			addFreeBlock(adrno);
			sizeRemoved=sizeRemoved+DISK_BLOCK_SIZE;
            }
            else{
                break;
            }
            addrblockno++;
		}
		if(rmSize>sizeRemoved)
		{

			unsigned long fblocksinglelinkno=0;
		    lseek(fd,2*DISK_BLOCK_SIZE+inodeNo*INODE_SIZE+12+8*4,SEEK_SET);
            read(fd,&fblocksinglelinkno,sizeof(fblocksinglelinkno));

			int noOfBlocksSingle=0;
			unsigned long arrayofadrblock[DISK_BLOCK_SIZE/SIZE_OF_ADDRESS];
			lseek(fd,fblocksinglelinkno*DISK_BLOCK_SIZE,SEEK_SET);
			read(fd,arrayofadrblock,sizeof(arrayofadrblock));
			while(noOfBlocksSingle<DISK_BLOCK_SIZE/SIZE_OF_ADDRESS)
			{
			    if(rmSize>sizeRemoved)
                {

				unsigned long fblockstage2=arrayofadrblock[noOfBlocksSingle];
				addFreeBlock(fblockstage2);
				sizeRemoved=sizeRemoved+DISK_BLOCK_SIZE;
                }
                else
                {
                    break;
                }
                noOfBlocksSingle++;
			}
		}


		if(rmSize>sizeRemoved)
		{
			unsigned long fblocktriplelinkno=0;
			lseek(fd,2*DISK_BLOCK_SIZE+inodeNo*INODE_SIZE+12+9*4,SEEK_SET);
            read(fd,&fblocktriplelinkno,sizeof(fblocktriplelinkno));
			int noOfBlocksfirsttriple=0;
			unsigned long arrayofFirstTripleAdrblock[DISK_BLOCK_SIZE/SIZE_OF_ADDRESS];
			lseek(fd,fblocktriplelinkno*DISK_BLOCK_SIZE,SEEK_SET);
			read(fd,arrayofFirstTripleAdrblock,sizeof(arrayofFirstTripleAdrblock));
			while((rmSize>sizeRemoved) && noOfBlocksfirsttriple<DISK_BLOCK_SIZE/SIZE_OF_ADDRESS)
			{
				unsigned long fblocktripleSecondlinkno=arrayofFirstTripleAdrblock[noOfBlocksfirsttriple];
				int noOfBlocksSecondtriple=0;
				unsigned long arrayofSecondTripleAdrblock[DISK_BLOCK_SIZE/SIZE_OF_ADDRESS];
				lseek(fd,fblocktripleSecondlinkno*DISK_BLOCK_SIZE,SEEK_SET);
				read(fd,arrayofSecondTripleAdrblock,sizeof(arrayofSecondTripleAdrblock));
				while((rmSize>sizeRemoved) &&noOfBlocksSecondtriple<DISK_BLOCK_SIZE/SIZE_OF_ADDRESS)
				{
					unsigned long fblocktripleThirdlinkno=arrayofSecondTripleAdrblock[noOfBlocksSecondtriple];
					int noOfBlocksThirdtriple=0;
					unsigned long arrayofThirdTripleAdrblock[DISK_BLOCK_SIZE/SIZE_OF_ADDRESS];
					lseek(fd,fblocktripleThirdlinkno*DISK_BLOCK_SIZE,SEEK_SET);
					read(fd,arrayofThirdTripleAdrblock,sizeof(arrayofThirdTripleAdrblock));
					while((rmSize>sizeRemoved) &&noOfBlocksThirdtriple<DISK_BLOCK_SIZE/SIZE_OF_ADDRESS)
					{
						unsigned long fblockfinal=arrayofThirdTripleAdrblock[noOfBlocksThirdtriple];
						addFreeBlock(fblockfinal);
						sizeRemoved=sizeRemoved+DISK_BLOCK_SIZE;
						noOfBlocksThirdtriple++;
					}
					addFreeBlock(fblocktripleThirdlinkno);
					noOfBlocksSecondtriple++;
				}
				addFreeBlock(fblocktripleSecondlinkno);
				noOfBlocksfirsttriple++;
			}
			addFreeBlock(fblocktriplelinkno);
		}
		inode.uid=dir.inode;
		inode.used=0;
		lseek(fd,2*DISK_BLOCK_SIZE+dir.inode*INODE_SIZE,SEEK_SET);
		write(fd,&inode,sizeof(inode));
		dir.inode = -2;
		strncpy(dir.file_name, "...", 14);
		lseek(fd, (currentInode.addr[0]*DISK_BLOCK_SIZE)+(file_count*16), SEEK_SET);
		write(fd, &dir, 16);
	}
}

int ls()
{
    lseek(fd,(currentInode.addr[0]*DISK_BLOCK_SIZE), SEEK_SET);
    struct fs_dir dir;
	int k;
    for(k=0;k<64;k++)
	{
        read(fd,&dir,sizeof(dir));
        if(dir.inode>0)
        {
            if((strcmp("..",dir.file_name)==0)|| (strcmp("...",dir.file_name)==0))
            {
                continue;
            }
            printf("%s\n",dir.file_name);
        }
        if(dir.inode==-1)
        {
            break;
        }
	}
	return 1;
}

int cd(char *filename)
{
    return opendir( fd,filename);
}

int opendir(int fd,char *filename)
{
    int file_count = 0;
	int flag = 0;
	struct fs_dir dir;
	int inodeNo;
	int count=0;
	lseek(fd,(currentInode.addr[0]*DISK_BLOCK_SIZE), SEEK_SET);
	read(fd, &dir, 16);
	while(dir.inode != -1 && count<64){
            if((strcmp(".",filename)==0))
            {
                if(dir.inode==0)
                {
                    printf("root directory opened\n");
                    //return 1;
                }
                lseek(fd,(2*DISK_BLOCK_SIZE + dir.inode*INODE_SIZE), SEEK_SET);
                read(fd, &currentInode, sizeof(currentInode));
                printf("parent directory opened\n");
                return 1;
            }
            if(checkifFile(filename))
            {
                printf("\n$$ ERROR!!!! NO directiory found$$\n");
                return 0;
            }
            if((strcmp("..",filename)==0))
            {
                return 0;
            }
		if(strcmp(filename, dir.file_name)==0){
			inodeNo = dir.inode;

			flag = 1;
			break;
		}
		file_count++;
		read(fd, &dir, 16);
		count++;
	}
	if(flag != 1){
		printf("\n$$ ERROR!!!! NO directiory found$$\n");
		return 1;
	}
	else
	{
        lseek(fd,(2*DISK_BLOCK_SIZE + dir.inode*INODE_SIZE), SEEK_SET);
        read(fd, &currentInode, sizeof(currentInode));
        return 1;
	}
}


int printsuper()
{//for internal check
    printf("fs1 isize=%d\n",super.isize);
    printf("fs1 fsize=%d\n",super.fsize);
    printf("fs1 nfree=%d\n",super.nfree);
}

int quit()
{
	lseek(fd,DISK_BLOCK_SIZE,SEEK_SET);
	write(fd,&super,sizeof(super));
	lseek(fd,2*DISK_BLOCK_SIZE,SEEK_SET);
	write(fd,&currentInode,sizeof(currentInode));
	return 1;
}


main()
{

	char line[1024];
	char cmd[1024];
	char arg1[1024];
	char arg2[1024];
	char arg3[1024];
	int inumber, args;

	while(1) {
		printf(prompt);
		printf(">");

		if(!fgets(line,sizeof(line),stdin)) break;

		if(line[0]=='\n') continue;
		line[strlen(line)-1] = 0;

		args = sscanf(line,"%s %s %s %s",cmd,arg1,arg2,arg3);
		if(args==0) continue;

		if(!strcmp(cmd,"initfs")) {
			if(args==4) {
				if(initfs(arg1,atoi(arg2),atoi(arg3))) {
					printf("File System Initialized.\n");
					printf("opened emulated disk image %s with %d blocks\n",arg1,disk_size());
				} else {
					printf("failed!\n");
				}
			} else {
				printf("use: initfs<const char *filename, int fsize,int ninodes >\n");
			}
		}else if(!strcmp(cmd,"cpin")) {
			if(args==3) {
				if(cpin(arg1,arg2)) {
					printf("File Copied from %s to v6 file :%s\n",arg1,arg2);
				} else {
					printf("File copy failed!\n");
				}
			} else {
				printf("use: cpin<char *filename> <char *v6filename >\n");
			}
		} else if(!strcmp(cmd,"cpout")) {
			if(args==3) {
				if(cpout(arg1,arg2)) {
					printf("File Copied from %s to ext file :%s\n",arg1,arg2);
				} else {
					printf("File copy failed!\n");
				}
			} else {
				printf("use: cpout <const char *v6filename> <char *extfile>\n");
			}
		} else if(!strcmp(cmd,"mkdir")) {
			if(args==2) {
				if(newdir(arg1)) {
					printf("directory created : %s \n",arg1);
				} else {
					printf("directory create failed!\n");
				}
			} else {
				printf("use: mkdir<char *dirname>\n");
			}
		}else if(!strcmp(cmd,"cd")) {
			if(args==2) {
				if(cd(arg1)) {
					printf("directory opened \n");
				} else {
					printf("directory open failed!\n");
				}
			} else {
				printf("use: cd<char *dirname>\n");
			}
		}else if(!strcmp(cmd,"rm")) {
			if(args==2) {
				if(rm(arg1)) {
					printf("file : %s removed \n",arg1);
				} else {
					printf("deleting file failed!\n");
				}
			} else {
				printf("use: rm<char *filename>\n");
			}
		}
		else if(!strcmp(cmd,"openfs")) {
			if(args==2) {
				if(openfs(arg1)) {
					printf("v6 filesystem : %s is initialized and ready to use \n",arg1);
				} else {
					printf("Error Opening V6 file system!\n");
				}
			} else {
				printf("use: openfs<const char *filename>\n");
			}
		}else if(!strcmp(cmd,"ls")) {
			if(ls()>0)
			{
				printf("cmd :help for cmd list\n");
				//printf("Bye Bye!!!\n");
			}
		}else if(!strcmp(cmd,"q")) {
			if(quit()>0)
			{
				printf("File System Modifications saved\n");
				printf("Bye Bye!!!\n");
			}
			break;
		}else if(!strcmp(cmd,"psuper")) {
			if(printsuper()>0)
			{
				printf("super\n");
			}
		}
		else if(!strcmp(cmd,"help")) {
			printf("To initiate a new V6 file system use: initfs\n");
			//printf("To open a existing V6 file system use: openfs\n");
			//printf("cmd: openfs<const char *filename>\n");
			printf("all the operations are subjected to level i.e \n");
            printf("until and unless u enter in to directory all operations are done in that directory only\n");
            printf("go to that directory  where operations are to be executed\n");
			printf("cmd: initfs<const char *filename, int fsize,int ninodes >\n");
			printf("ex:initfs C:\\Users\\himas\\Desktop\\test8.data 500 100\n");
			printf("cmd: cpin<char *filename> <char *v6filename >\n");
			printf("ex: cpin C:\\Users\\himas\\Desktop\\test.txt v6txt.txtn\n");
			printf("cmd: cpout <const char *v6filename> <char *extfile>\n");
			printf("ex: cpout v6txt.txt C:\\Users\\himas\\Desktop\\tout.txt\n");
			printf("cmd: mkdir<char *dirname>\n");
			printf("ex: mkdir dir_name\n");
			printf("cmd: rm<char *filename>\n");
			printf("ex: rm dir_name\n");
			printf("cmd: ls for list of files and directories at that level\n");
			printf("ex: ls\n");
			printf("cmd: cd <dir name> to open directory\n");
			printf("ex: cd dir_name\n");
			printf("cmd: cd <.> to open parent directory\n");
			printf("ex: cd .\n");
			printf("cmd: q to quit\n");
			printf("ex: q\n");
		}
		else {
			printf("unknown command: %s input q to exit  or help for commands\n",cmd);
		}
	}
}