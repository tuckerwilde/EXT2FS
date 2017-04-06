#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <ext2fs/ext2_fs.h> 

#include "util.c"
#include "type.h"
#include "iget_iput_getino.c"  // YOUR iget_iput_getino.c file with
                               // get_block/put_block, tst/set/clr bit functions

char *disk = "mydisk";
char line[128], cmd[64], pathname[64];
char buf[BLKSIZE];              // define buf1[ ], buf2[ ], etc. as you need

//Initializer
int init()
{
	for (int i = 0; i < NMINODE; i++)
	{
		minode[i].refCount = 0;
	}

	proc[0].pid = 1;
	proc[0].uid = 0;
	proc[0].cwd = 0;
	for (int i = 0; i < NFD; i++)
	{
		proc[0].fd[i] = NULL;
		proc[1].fd[i] = NULL;
	}
	printf("Proc 0 initialized\n");

	proc[1].pid = 2;
	proc[1].uid = 1;
	proc[1].cwd = 0;
	printf("Proc 1 initialized\n");
	return;
}

int mount_root()
{
	printf("Mount Root.\n");
	root = iget(dev,2);
}


int ls()
{
	int tempIno, i = 0;
	char *cp;
	int dev = running->cwd->dev;
	MINODE *mip = running->cwd;
	char buff[1024], buff2[1024];
	
	if (strcmp(pathname, "") != 0)
	{
		if (pathname[0] == '/')
		{
			dev = root->dev;
		}

		tempIno = getino(&dev, pathname);

		mip = iget(dev, tempIno);
	}
	else
	{
		tempIno = running->cwd->ino;
		mip = running->cwd;
	}

	get_block(mip->dev, mip->INODE.i_block[0], buff);
	dp = (DIR *)buff;
	cp = buff;

	while (cp < &buff[1024])
	{
		strncpy(buff2, dp->name, dp->name_len);
		buff2[dp->name_len] = 0;
		printf("%s ", buff2);
		cp += dp->rec_len;
		dp = (DIR *)cp;
	}
	printf("\n");
	if (mip != running->cwd)
	{
		iput(mip);
	}
}

int chdir()
{
	int tempIno;
	MINODE *mip;
	//First check if the path name is good.
	//This isn't necessary. Remove.
	if (strcmp(pathname, "") != 0){

		//Is it absolute? If so, set the device to roots.
		if (pathname[0] == '/')
		{
			dev = root->dev;
		}
		else
		{
			//Else. Check where we are at, and assign that device.
			dev = running->cwd->dev;
		}

		//Let's get the ino.
		tempIno = getino(&dev, pathname);

		if (tempIno == 0)
		{
			printf("This pathname: %s does not exist.\n", pathname);
			return;
		}

		mip = iget(dev, tempIno);

		if (S_ISDIR(mip->INODE.i_mode))
		{
			iput(running->cwd);
			running->cwd = mip;
		}
		else
		{
			iput(mip);
			printf("Cannot CD into a file...\n");
		}
	}
	else
	{
		iput(running->cwd);
		running->cwd = iget(dev,2);
	}
}

int pwd()
{
	MINODE *hold;
	//Root check
	if (running->cwd->ino == 2)
	{
		//Just do a quick thing...
		printf("/");
	}
	//Else...
	hold = running->cwd;
	//Enter recursive function...
	pwdPrint(hold);
	running->cwd = hold;
	printf("\n");
}

main(int argc, char *argv[ ])
{
	//Check for the input
	if (argc > 1)
		disk = argv[1];

	if ((dev = fd = open(disk, O_RDWR)) < 0)
	{
		printf("Open %s failed\n", disk);
	}

	printf("DEV: %d\t FD: %d\n", dev, fd);

	printf("Quick check if disk is EXT2...\n");

	//buf used
	get_block(fd, 1, buf);
	sp = (SUPER *)buf;

	if(sp->s_magic != 0xEF53)
	{
		printf("%x %s is not an EXT2 FS\n", sp->s_magic, dev);
		exit(1);
	}

	ninodes = sp->s_inodes_count;
	nblocks = sp->s_blocks_count;
	
	//Group descripter
	get_block(fd, 2, buf);
	gp = (GD *)buf;

	//grab vars
	bmap = gp->bg_block_bitmap;
	imap = gp->bg_inode_bitmap;
	iblock = gp->bg_inode_table;

	//print vals
	printf("ninodes: %d\t nblocks: %d\t bmap: %d\t imap: %d\t iblock: %d\t",
		ninodes, nblocks, bmap, imap, iblock);

	//Initialized processes
	init();

	mount_root();

	proc[0].cwd = iget(root->dev, 2);
	proc[1].cwd = iget(root->dev, 2);

	running = &proc[0];

	while(1){
		
		char inputT[128];
		char *token, *hold;
		strcpy(pathname, "");
		printf("input command: [ls|cd|pwd|quit] $ ");

		//grab command
		fgets(inputT, 128, stdin);

		inputT[strlen(inputT)-1] = 0;

		token = strtok(inputT, " ");

		//Pretty janky fix to no fucking pathname.
		strcpy(cmd,token);
		hold = strtok(NULL, "");

		if (!hold)
		{
			strcpy(pathname, "");
		}
		else
		{
			strcpy(pathname, hold);
		}
		
		printf("cmd: %s\t pathname: %s\n\n", cmd, pathname);

       if (strcmp(cmd, "ls")==0)
          ls();
       if (strcmp(cmd, "cd")==0)
          chdir();
       if (strcmp(cmd, "pwd")==0)
          pwd();
       //if (strcmp(cmd, "quit")==0)
          //quit();		
	}

}