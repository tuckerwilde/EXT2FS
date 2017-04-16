/*
                                                 ,  ,
                                               / \/ \
                                              (/ //_ \_
     .-._                                      \||  .  \
      \  '-._                            _,:__.-"/---\_ \
 ______/___  '.    .--------------------'~-'--.)__( , )\ \
`'--.___  _\  /    |             Here        ,'    \)|\ `\|
     /_.-' _\ \ _:,_          Be Dragons           " ||   (
   .'__ _.' \'-/,`-~`                                |/
       '. ___.> /=,|  Abandon hope all ye who enter  |
        / .-'/_ )  '---------------------------------'
        )'  ( /(/
             \\ "
              '=='

This monstrosity was built to do one thing, and one thing only:
	
					Get the fuck out of 360.
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <ext2fs/ext2_fs.h> 
#include <time.h>

#include "util.c"
#include "type.h"
#include "iget_iput_getino.c"  // YOUR iget_iput_getino.c file with
                               // get_block/put_block, tst/set/clr bit functions

char *disk = "mydisk";
char line[128], cmd[64], pathname[64], parameter[64];
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
	printf("Name\t Rec_Len Ino\n");
	while (mip->INODE.i_block[i])
	{
		get_block(dev, mip->INODE.i_block[i], buff);
		cp = buff;
		dp = (DIR *)buff;
		while (cp < &buff[1024])
		{
			strncpy(buff2, dp->name, dp->name_len);
			buff2[dp->name_len] = 0;
			printf("%s\t %d\t %d\n", buff2, dp->rec_len,dp->inode);
			cp += dp->rec_len;
			dp = (DIR *)cp;
		}
		i++;
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

/*
	This function recursively prints the current working
	directory. 
	1. Checks for root, otherwise grab cwd
	2. Enter print function, utilizing the parent ino in '..'
	3. Checkout the MINODE of the parent. Make that our new CWD.
	4. Recurse back in until done.
	5. Then print the 'child' utilizing the childPrint helper.
*/
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

/*
	This function creates a new directory
*/
int make_dir()
{
	MINODE *mip, *pip;
	char dirN[256], basN[256];
	char *parent, *child;
	if (pathname[0] == "/")
	{
		//Its absolute
		mip = root;
		dev = root->dev;
	}
	else
	{
		//Assume it's relative.
		mip = running->cwd;
		dev = running->cwd->dev;
	}
	//The function that destroys original copy.
	//So make more copies...
	//Also this may cause Seg Faults.. so come back to this.
	strcpy(dirN, pathname);
	strcpy(basN, pathname);
	//Assign those copies.
	parent = dirname(dirN);
	child = basename(basN);

	//Grab MINODE of parent.
	//May have to tokenize this.. we will see.
	printf("dev in make_dir %d\n", dev);
	int pino = getino(&dev, parent);
	pip = iget(dev, pino);

	//TODO:
	//Verification for...
	//(1). Is it a DIR?
	//(2). Child doesn't already exist.
	mymkdir(pip, child);

	//Touch time
	pip->INODE.i_atime = time(0L);
	pip->INODE.i_links_count += 1;
	pip->dirty = 1;
	pip->INODE.i_mode = 0040000;

	iput(pip);

}


int creat_file()
{
	MINODE *mip, *pip;
	int tempIno;
	char dirN[256], basN[256];
	char *parent, *child;
	if (pathname[0] == "/")
	{
		//Its absolute
		mip = root;
		dev = root->dev;
	}
	else
	{
		//Assume it's relative.
		mip = running->cwd;
		dev = running->cwd->dev;
	}
	//The function that destroys original copy.
	//So make more copies...
	//Also this may cause Seg Faults.. so come back to this.
	strcpy(dirN, pathname);
	strcpy(basN, pathname);
	//Assign those copies.
	parent = dirname(dirN);
	child = basename(basN);

	//Grab MINODE of parent.
	//May have to tokenize this.. we will see.
	int pino = getino(&dev, parent);
	pip = iget(dev, pino);

	//TODO:
	//Verification for...
	//(1). Is it a DIR?
	//(2). Child doesn't already exist.
	my_creat(pip, child);

	//Touch time
	pip->INODE.i_atime = time(0L);
	pip->INODE.i_links_count = 1;
	pip->dirty = 1;

	iput(pip);
}

int rmdir()
{
	int temp_ino, par_ino;
	MINODE *mip, *pip;
	char buff[BLKSIZE];
	char *cp;

	//Determine dev.
	//Ino and MINODE of pathname
	temp_ino = getino(&dev, pathname);
	mip = iget(dev, temp_ino);

	//TODO: Check ownership.

	//Check for valid path
	if (temp_ino == 0)
	{
		printf("This isn't a valid pathname\n");
	}
	//Do quick checks.
	if (S_ISREG(mip->INODE.i_mode))
	{
		printf("This isn't a directory\n");
		return;
	}
	//Check for empty?
	get_block(dev, mip->INODE.i_block[0], buff);
	//Could check that the parent '..' is 1012?
	dp = (DIR *)buff;
	cp = buff;
	cp += dp->rec_len;
	dp = (DIR *)cp;
	//This should give us parent inode.
	par_ino = dp->inode;
	if (dp->rec_len != 1012)
	{
		printf("Directory is not empty\n");
		return;
	}

	//ALL CHECKS COMPLETE. Move on.
	//Utlize fancy new truncate function. Clear this certain
	// block then work on removing the name from parent
	truncate(mip);
	idealloc(dev, mip->ino);
	//put the minode back
	iput(mip);
	pip = iget(dev, par_ino);

	//Call the helper function
	rm_child(pip, basename(pathname));

	//Decrements
	pip->INODE.i_links_count--;
	pip->INODE.i_atime = time(0L);
	pip->dirty = 1;
	iput(pip);
	return;


}


int hard_link()
{
	//Get INO of pathname.
	int temp_ino, par_ino;
	MINODE *mip, *tip;
	char parent[64], child[64];
	char *basen, *dirn;

	temp_ino = getino(&dev, pathname);
	printf("Received INO of %d\n", temp_ino);

	if (temp_ino == 0)
	{
		printf("This pathname doesn't exist...\n");
		return;
	}

	//Get the MINODE corresponding to that ino.
	mip = iget(dev, temp_ino);

	//Quick check for smartness.
	if (S_ISDIR(mip->INODE.i_mode))
	{
		printf("Can't link a directory...\n");
		return;
	}

	//Dissect parameter into base and dir
	strcpy(parent, parameter);
	strcpy(child, parameter);
	//Example: /a/b/c
	basen = dirname(parent); // /a/b
	dirn = basename(child); // cs


	//Check /x/y exists
	par_ino = getino(&dev, basen);
	if (par_ino == 0)
	{
		printf("Parent doesn't exist\n");
		return;
	}

	tip = iget(dev, par_ino);
	enter_name(tip, temp_ino, dirn);
	tip->INODE.i_links_count++;

	iput(tip);
	iput(mip);
}

//TODO: Finish unlink...

int unlink()
{
	int path_ino, par_ino;
	char *basen, *dirn, temppath1[128], temppath2[128];
	MINODE *mip, *pip;

	path_ino = getino(&dev, pathname);
	mip = iget(dev, path_ino);

	//Verify it's a file
	if (S_ISDIR(mip->INODE.i_mode))
	{
		printf("Can't unlink a directory...\n");
		return;
	}

	mip->INODE.i_links_count--;

	//This is a check to see if the file has zero connections
	//Essentially a delete.
	if (mip->INODE.i_links_count == 0)
	{
		truncate(mip);
		idealloc(dev, mip->ino);
	}


	strcpy(temppath1,pathname);
	strcpy(temppath2,pathname);
	basen = basename(temppath1);
	dirn = dirname(temppath2);

	printf("Basename: %s\n", basen);

	par_ino = getino(&dev, dirn);
	pip = iget(dev, par_ino);

	rm_child(pip, basen);
	iput(pip);
	iput(mip);
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
		printf("input command: [ls|cd|pwd|mkdir|creat|link|quit] $ ");

		//grab command
		fgets(inputT, 128, stdin);

		inputT[strlen(inputT)-1] = 0;

		token = strtok(inputT, " ");

		//Pretty janky fix to no fucking pathname.
		strcpy(cmd,token);
		hold = strtok(NULL, " ");

		if (!hold)
		{
			strcpy(pathname, "");
		}
		else
		{
			strcpy(pathname, hold);
		}
		

		//For link...
		hold = strtok(NULL, "");
		
		if (hold)
			strcpy(parameter, hold);

		if (strcmp(parameter, "")==0)
			printf("cmd: %s\t pathname: %s\n\n", cmd, pathname);
		else
			printf("cmd: %s\t pathname: %s\t parameter: %s\n\n", cmd, pathname, parameter);

		if (strcmp(cmd, "ls")==0)
			ls();
		if (strcmp(cmd, "cd")==0)
			chdir();
		if (strcmp(cmd, "pwd")==0)
			pwd();
		if (strcmp(cmd, "mkdir")==0)
			make_dir();
		if (strcmp(cmd, "creat")==0)
			creat_file();
		if (strcmp(cmd, "rmdir")==0)
			rmdir();
		if (strcmp(cmd, "link")==0)
			hard_link();
		if (strcmp(cmd, "unlink")==0)
			unlink();

		strcpy(parameter, "");
	}

}