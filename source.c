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
#define KRED  "\x1B[31m"
#define RESET "\x1B[0m"

char *disk = "mydisk";
char line[512], cmd[256], pathname[256], parameter[256];
char buf[BLKSIZE];              // define buf1[ ], buf2[ ], etc. as you need

//Initializer
/*
*Function: init
*--------------
*Initializes our two running processes, assigning all minodes
*ref counts to zero, as well as setting pid, uid, and CWD of each
*process and setting up the FD's for future use.
*
*Accepts no input, other than global variables.
*
*Outputs nothing other than print.
*/
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

int quit()
{
	for (int i = 0; i < NMINODE; i++)
	{
		//Means it's still in use.
		if (minode[i].refCount >= 1)
		{
			iput(&minode[i]);
			minode[i].refCount = 1;
		}
	}
	printf("Exit Successful.\n");
	exit(100);
}

/************* LEVEL 1 FUNCTIONS START **********************/
/*
*Function: ls
*------------
*Mimics the linux LS call. Logic behind it is simple, and is as follows
*1. Grab a MINODE mip, assign it to the CWD of the running process
*2. Quick step: Check if pathname is empty.
*2.1 If not empty, call getino to search for the dirname of the pathname and set mip to that
*2.2 If empty, continue with mip set at running cwd
*3. Assuming direct blocks only, step through each one, like search
*4. If a block is empty, we've hit the end.
*
*Returns no value, only prints out the contents of each block
*/
int ls()
{
	int tempIno, i = 0;
	char *cp;
	int dev = running->cwd->dev;
	MINODE *mip = running->cwd, *tip;
	char buff[BLKSIZE], buff2[BLKSIZE];
	
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
	printf("Rec_Len  Ino\t SIZE\t name\n");
	while (mip->INODE.i_block[i] != 0)
	{
		get_block(dev, mip->INODE.i_block[i], buff);
		cp = buff;
		dp = (DIR *)buff;
		while (cp < &buff[1024])
		{
			tip = iget(dev, dp->inode);
			strncpy(buff2, dp->name, dp->name_len);
			buff2[dp->name_len] = 0;
			printf("%d\t %d\t %d\t %o\t %s", dp->rec_len ,dp->inode, tip->INODE.i_size,tip->INODE.i_mode ,buff2);
			if (tip->INODE.i_mode == 0xA000)
				printf(" =SL=> %s\n", tip->INODE.i_block);
			else
				printf("\n");
			iput(tip);
			cp += dp->rec_len;
			dp = (DIR *)cp;
		}
		i++;
		//THIS ALMOST KILLED ME. DONT DO THIS.
		//put_block(dev, mip->INODE.i_block[i], buff);
	}


	printf("\n");
	if (mip != running->cwd)
	{
		iput(mip);
	}
}


//These cause some seriously weird errors. Will come back to them after Level 2.
/*
int touch()
{
	char buff[BLKSIZE];
	int temp_ino;
	MINODE *mip;

	if (pathname[0] == '/')
	{
		dev = root->dev;
	}
	else
	{
		//Else. Check where we are at, and assign that device.
		dev = running->cwd->dev;
	}

	temp_ino = getino(&dev, pathname);
	mip = iget(dev, temp_ino);

	mip->INODE.i_atime = time(0L);
	printf("%s touched\n",pathname);

	iput(mip);
}

int ch_mod()
{
	printf("Do we enter the function\n");
	int temp_ino;
	MINODE *mip;

	sscanf(pathname, "%o", &mode);

	if (parameter[0] == '/')
	{
		dev = root->dev;
	}
	else
	{
		//Else. Check where we are at, and assign that device.
		dev = running->cwd->dev;
	}

	printf("Dec %d, oct %o\n", mode, mode);
	temp_ino = getino(&dev, parameter);
	mip = iget(dev, temp_ino);

	mip->INODE.i_mode = mode;

	iput(mip);


}
*/
/*
*Function: change directory
*--------------------------
*mimics the linux command cd, changes directory to pathname or back to root if !pathname
*
*1. Check for absolute or relative, assigning dev.
*2. Call getino function, which parses out pathname and checks if that dirname exists, if so
*3. Return the ino, and grab the MINODE associated
*4. Simply assign our CWD to that minode, with that associated INO
*
*Returns nothing, just changes our current working directory in the active process
*/
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
	Function: Mkdir, mimics the linux command mkdir. Creates a new directory allocating memory.
	-------------------------------------------------------------------------------------------
	1. Absolute, relative check, assigning our MINODE as necessary.
	2. Grab the parent ino after calling dir and base name. Then grab the minode associated with parent.
	3. Call mymkdir, which calls enter_name, allocating memory and moving information as necessary
	4. touch time, linkscount, marking dirty, and setting mode. put back our minodes.

	Returns nothing, just creates a directory, allocating space as neccessary.
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
	int pino = getino(&dev, parent);
	printf("Parent INO: %d\n", pino);
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

/*
	Function: creat, mimics the creat function in linux. Creates a file.
	--------------------------------------------------------------------

	1. Extremely similar process to mkdir, only thing changed is the mode.
	2. And the helper function.
*/
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

/*
	Function: Remove directory
	--------------------------
	1. Grab our minode, mip, using pathname.
	2. Some safety checks, valid path, directory... blah blah blah.
	3. Grab the block of the minode, the first, simply to check if it's empty. If '..' is not 1012, its not empty
	4. Truncate our minode, deallocating all the blocks.
	5. Deallocate the INODE.
	6. Go into the parent directory, and deal with that. Further explained at the helper function.
	7. Touch parents time, and mark dirty.
*/
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
	printf("Dirn:%s\n", dirn);
	getchar();
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

int symlink()
{
	//assuming oldname (pathname) only has 60 chars...
	char oldname[84],temppath2[128], temppath1[128];
	char *basen, *dirn, *write_in;
	int old_ino, par_ino;
	MINODE *pip, *mip;
	//This is the pathname, i.e /a/b/c
	strncpy (oldname, pathname, 60);

	//Let's get base and dir
	strcpy(temppath1, parameter);
	strcpy(temppath2, parameter);
	dirn = dirname(temppath1);
	basen = basename(temppath2);

	old_ino = getino(&dev, parameter);
	if (old_ino != 0)
	{
		printf("This file already exists\n");
		return;
	}
	//This means they are the same and not absolute.
	printf("Dirn: %s basen: %s param: %s\n",dirn, basen, parameter);
	if (strcmp(basen, parameter) == 0)
	{
		par_ino = running->cwd->ino;
		pip = iget(dev, par_ino);
		my_creat(pip, basen);
	}
	else
	{
		par_ino = getino(&dev, dirn);

		pip = iget(dev, par_ino);
		printf("\n\npar_ino:%d dirn: %s\n", par_ino, dirn);
		my_creat(pip, basen);
	}

	//Write oldname into that new file.
	old_ino = getino(&dev, parameter);
	mip = iget(dev, old_ino);
	mip->INODE.i_mode = 0xA000;
	//write_in = (char*)mip->INODE.i_block;
	strcpy(mip->INODE.i_block, pathname);
	iput(mip);
}

int readlink()
{
	int temp_ino;
	char *cp;
	MINODE *mip;

	temp_ino = getino(&dev, pathname);

	if (temp_ino == 0)
	{
		printf("This path does not exist.\n");
		return;
	}
	mip = iget(dev, temp_ino);
	
	if (mip->INODE.i_mode != 0xA000)
	{
		printf("This is not a symbolic link\n");
		return;
	}
	
	printf("%s\n",mip->INODE.i_block);

	printf("\n");

}

/************* LEVEL 1 FUNCTIONS END **********************/
/************* LEVEL 2 FUNCTIONS START **********************/
int open_file()
{
	/*
	Our inputs will be
	->Pathname: the file to open
	->parameter: tells us to R|W|RW|APPEND, 0,1,2,3 respectively
	*/
	int temp_ino, i, mode;
	MINODE *mip;
	OFT *oftp = malloc(sizeof(OFT));

	mode = atoi(parameter);
	
	if (pathname[0] == '/')
	{
		//Absolute
		dev = root->dev;
	}
	else
	{
		//Relative
		running->cwd->dev;
	}

	temp_ino = getino(&dev, pathname);
	if (temp_ino == 0)
	{
		printf("File doesn't exist\n");
		return;
	}

	mip = iget(dev, temp_ino);
	
	//TODO: Check permissions.
	if (!S_ISREG(mip->INODE.i_mode))
	{
		printf("Cannot open a directory\n");
		return;
	}

	//TODO: Check whether the file is ALREADY opened with INCOMPATIBLE mode.
	oftp->mode = mode;
	oftp->refCount = 1;
	oftp->mptr = mip;

	switch(mode)
	{
		case 0: oftp->offset = 0; // R: offset = 0
				break;
		case 1: truncate(mip); // W: truncate file to 0 size
				oftp->offset = 0;
				break;
		case 2: oftp->offset = 0; // RW: do NOT truncate file
				break;
		case 3: oftp->offset = mip->INODE.i_size; // APPEND mode
				break;
		default: printf("Invalid Mode\n");
				return -1;
	}
	//Find first FD that is open.
	for (i = 0; i < NFD; i++)
	{
		if (running->fd[i] == NULL)
		{
			running->fd[i] = (oftp);
			break;
		}
	}

	//Update time
	if ((int)parameter == 0)
	{
		mip->INODE.i_atime = time(0L);
		mip->dirty = 1;
	}
	else
	{
		mip->INODE.i_atime = mip->INODE.i_mtime = time(0L);
		mip->dirty = 1;
	}
	//We made it this far?? Really? That's impressive.
	//This returns the FD.
	hold_fd = i;
	printf("File Descriptor opened: %d, MODE: %d\n", i, oftp->mode);
	return i;
}

int close_file()
{
	int fd_close;
	OFT *oftp = malloc(sizeof(OFT));
	MINODE *mip;

	fd_close = atoi(pathname);
	printf("close: %d\n", fd_close);
	
	if (fd_close > NFD)
	{
		printf("Out of range of FDs\n");
		return -1;
	}

	//Check for NULL pointer.
	if (running->fd[fd_close] == NULL)
	{
		printf("This FD is not in use\n");
		return -1;
	}

	oftp = (running->fd[fd_close]);
	running->fd[fd_close] = 0;
	oftp->refCount--;
	
	if (oftp->refCount > 0)
	{
		return 0;
	}

	mip = oftp->mptr;
	iput(mip);
	printf("Closed FD %d\n", fd_close);
	

}

int l_seek(int fd, int offset)
{

}

int pfd()
{
	//TODO: represent mode in char form
	printf("FD\tMODE\tOFFSET\tINODE\n");
	printf("================================\n");
	for (int i = 0; i < NFD; i++)
	{

		if (running->fd[i] == NULL)
		{
			continue;
		}
		else
		{
			printf("%d\t%d\t%d\t%d\n", i, running->fd[i]->mode, running->fd[i]->offset, running->fd[i]->mptr->ino);
		}
	}
	printf("================================\n");
}

int read_file()
{
	//Check for read or write.
	//fd and #of bytes
	int temp_fd, n_bytes;
	char buff[BLKSIZE];
	OFT *oftp;

	temp_fd = atoi(pathname);
	n_bytes = atoi(parameter);

	oftp = running->fd[temp_fd];

	if (oftp == 0)
	{
		printf("Empty FD\n");
		return -1;
	}
	
	if (oftp->mode == 1 || oftp->mode == 3)
	{
		printf("Incompatible mode\n");
		return -1;
	}
	
	oftp->refCount++;

	printf("BYTES READ: %d, from FD %d\n",(myread(temp_fd,buff,n_bytes)), temp_fd);
	oftp->refCount--;

}

int write_file()
{
	//need fd and a text string
	char text[BLKSIZE];
	memset(text, 0, BLKSIZE);
	strncpy(text, parameter, strlen(parameter));

	char buff[BLKSIZE];
	memset(buff, 0, BLKSIZE);

	int temp_fd = atoi(pathname);

	OFT *oftp = running->fd[temp_fd];

	if (oftp->mode == 0 || oftp->mode == 2)
	{
		printf("Incompatible Mode\n");
		return -1;
	}

	if (oftp == 0)
	{
		printf("Invalid FD\n");
		return -1;
	}

	strncpy(buff, text, strlen(text));

	printf("String to write: '%s', length of string:%d\n", buff, strlen(buff));
	mywrite(temp_fd, buff, strlen(buff));

}

//TODO: Fix this? Doesn't work with my shit.
int mv_file()
{
	//input should be
	//mv src dest
	//mv pathname parameter
	char src[128], dest[128];
	strcpy(src, pathname);
	strcpy(dest, parameter);
	int srcino = getino(&dev, pathname);
	if(srcino == 0)
	{
		printf("Source not found\n");
		return -1;
	}
	//int destino = getino(&dev, dest);
	
	hard_link();
	unlink();
}

int cp_file()
{
	int temp_fd, temp_gd, bytes;
	char source[BLKSIZE] = {0}, dest[BLKSIZE] = {0}, buff[BLKSIZE];

	strncpy(source, pathname, strlen(pathname));
	strncpy(dest, parameter, strlen(parameter));
	printf("source: %s dest: %s\n", source, dest);

	//need to open source for read, 0.
	strcpy(parameter, "");
	strcpy(parameter, "1");
	strncpy(pathname, source, strlen(source));
	temp_gd = open_file();
	getchar();
	//open destination for wr.
	//TODO: creat first.
	//assume it's created.
	strcpy(pathname, "");
	strncpy(pathname, dest, strlen(dest));
	strcpy(parameter, "0");
	temp_gd = open_file();

	//test print
	pfd();

	while(bytes = myread(temp_gd, buff, BLKSIZE))
	{
		printf("%d Count\n", bytes);
		mywrite(temp_fd, buff, BLKSIZE);
		memset(buff, 0, BLKSIZE);
	}

}
/************* LEVEL 2 FUNCTIONS END **********************/

int menu()
{
	printf("WELCOME TO TUCKER AND JORDANS 360 PROJECT\n");
	printf("=========================================\n");
	printf("Commands:--------------------------------\n");
	printf("[ls|cd|pwd|mkdir|creat|rmdir|link|unlink|symlink|open|close|quit]\n");
	printf("-------------------------------------:End\n");
	printf("=========================================\n");
	return 0;
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
		
		char inputT[1024];
		char *token, *hold;
		strcpy(pathname, "");
		printf("input command: "KRED"$ "RESET);

		//grab command
		fgets(inputT, 1024, stdin);

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
		{
			strcpy(parameter, hold);
		}

		if (strcmp(parameter, "")==0)
			printf("cmd: %s\t pathname: %s\n\n", cmd, pathname);
		else
			printf("cmd: %s\t pathname: %s\t parameter: %s\n\n", cmd, pathname, parameter);

		if (strcmp(cmd, "menu") == 0)
			menu();
		if (strcmp(cmd, "ls")==0)
			ls();
		/*
		if (strcmp(cmd, "touch") == 0)
			touch();
			*/
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
		if (strcmp(cmd, "symlink") == 0)
			symlink();
		if (strcmp(cmd, "readlink") == 0)
			readlink();
		if (strcmp(cmd, "open") == 0)
			open_file();
		if (strcmp(cmd, "close") == 0)
			close_file();
		if (strcmp(cmd, "pfd") == 0)
			pfd();
		if (strcmp(cmd, "read") == 0)
			read_file();
		/*
		if (strcmp(cmd, "chmod") == 0)
			ch_mod();
			*/
		if (strcmp(cmd, "write") == 0)
			write_file();
		if (strcmp(cmd, "mv") == 0)
			mv_file();
		if (strcmp(cmd, "cp") == 0)
			cp_file();
		if (strcmp(cmd, "quit") == 0)
			quit();


		strcpy(parameter, "");
	}

}