#ifndef _UTILC_
#define _UTILC_

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ext2fs/ext2_fs.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>

#include "iget_iput_getino.c"
#include "type.h"

int pwdPrint(MINODE *hold)
{
	MINODE *temp;
	//Let's grab the parent!!
	char *pops = "..";


	//Quick check
	if (hold->ino == 2)
	{
		//Were in root...
		return;
	}
	
	//Grab the ino.
	int tempIno = getino(&hold->dev, pops);
	//Checkout the MINODE
	temp = iget(hold->dev, tempIno);
	//Let's temporarily make this the cwd
	running->cwd = temp;
	//Now.. Recursive...
	pwdPrint(temp);

	//Lets exit that...
	//Helper function to print out the children.
	childPrint(temp, hold->ino);
	iput(temp);
}

int childPrint (MINODE *temp, int ino)
{
	
	char *cp;
	char buff[BLKSIZE];

	//Reach up, grab that shit.
	ip = &(temp->INODE);

	//Lets look through the direct blocks only..
	//TODO: Maybe implement indirect? Talk to KCW I guess.
	for (int i = 0; i < 12; i++)
	{
		//First check if it's done?
		if (ip->i_block[i] == 0)
			return;

		get_block(dev, ip->i_block[i], buff);
		//step through.
		dp = (DIR *)buff;
		cp = dp;

		while (cp < &buff[1024])
		{
			//Have we found it?
			if (dp->inode == ino)
			{
				//char *holder = dp->name;
				//holder[dp->name_len] = 0;
				//Fix that ending problem!
				printf("/%s", dp->name);
			}
			//Do I keep going?
			cp += dp->rec_len;
			dp = (DIR *)cp;
		}
	}
	return;
}

int decFreeBlocks(int dev)
{
  char buf[BLKSIZE];

  // dec free inodes count in SUPER and GD
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_blocks_count--;
  put_block(dev, 1, buf);

  get_block(dev, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_blocks_count--;
  put_block(dev, 2, buf);
}

int balloc(int dev)
{
  int i;
  char buf[BLKSIZE];

  // read inode_bitmap block
  get_block(dev, bmap, buf);

  for (i=0; i < nblocks; i++){
    if (tst_bit(buf, i)==0){
      set_bit(buf,i);
      put_block(dev, bmap, buf);
      decFreeBlocks(dev);

      printf("Block Allocated:%d\n", i+1); 

      return i+1;
    }
  }
  printf("Balloc(): no more free inodes\n");
  return 0;
}

int decFreeInodes(int dev)
{
  char buf[BLKSIZE];

  // dec free inodes count in SUPER and GD
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_inodes_count--;
  put_block(dev, 1, buf);

  get_block(dev, 2, buf);
  gp = (GD *)buf;
  gp->bg_free_inodes_count--;
  put_block(dev, 2, buf);
}

int ialloc(int dev)
{
  int  i;
  char buf[BLKSIZE];

  // read inode_bitmap block
  get_block(dev, imap, buf);

  for (i=0; i < ninodes; i++){
    if (tst_bit(buf, i)==0){
       set_bit(buf,i);
       decFreeInodes(dev);

       put_block(dev, imap, buf);

       return i+1;
    }
  }
  printf("ialloc(): no more free inodes\n");
  return 0;
}

int mymkdir(MINODE *pip, char *name)
{
	printf("Do we make it into mymkdir?\n");
	int tempIno, tempBno;
	MINODE *mip;
	char buff[BLKSIZE];
	char *cp;

	//Lets get some memory for these fools.
	printf("char name: %s", name);
	tempIno = ialloc(dev);
	tempBno = balloc(dev);

	//Get that memory.
	mip = iget(dev, tempIno);

	//Temporary inode, let's fill out the new memory slot.
	INODE *tip = &mip->INODE;

	tip->i_mode = 0x41ED;
	tip->i_uid = running->uid;
	tip->i_gid = 0;
	tip->i_size = BLKSIZE;
	tip->i_links_count = 2;
	tip->i_atime = tip->i_ctime = tip->i_mtime = time(0L); //this may break..
	tip->i_blocks = 2;
	tip->i_block[0] = tempBno;

	for (int i = 1; i < 15; i++)
	{
		tip->i_block[i] = 0;
	}

	mip->dirty = 1;
	iput(mip);

	//Create the first two entries... God I should remember how to do this.
	get_block(dev, tip->i_block[0], buff);

	dp = (DIR *)buff;
	cp = dp;

	//"."
	dp->inode = tempIno;
	dp->rec_len = 12;
	dp->name_len = 1;
	strcpy(dp->name, ".");

	cp += dp->rec_len;
	dp = (DIR *)cp;

	//".."
	dp->inode = pip->ino;
	dp->rec_len = BLKSIZE-12;
	dp->name_len = 2;
	strcpy(dp->name, "..");

	//Write the data block back.
	put_block(dev, tempBno, buff);
	enter_name(pip, tempIno, name);


}

int enter_name(MINODE *pip, int myino, char *myname)
{
	printf("Do we make it into here?\n");
	char buff[BLKSIZE], buff2[BLKSIZE];
	char *cp;

	for (int i = 0; i < 13; i++)
	{
		if (pip->INODE.i_block[i] == 0)
			break;
		get_block(pip->dev, pip->INODE.i_block[i], buff);

		dp = (DIR *)buff;
		cp = dp;

		while (cp + dp->rec_len < buff + BLKSIZE)
		{
			//Todo: PRINT HERE. Too tired.
			cp += dp->rec_len;
			dp = (DIR *)cp;
		}
		//Dp is now at final spot.
		int needLen = (4*((8+dp->name_len+3)/4));
		int remain = (dp->rec_len) - needLen;

		if (remain >= needLen)
		{
			dp->rec_len = needLen;
			cp += dp->rec_len;
			dp = (DIR *)cp;
			dp->inode = myino;
			dp->rec_len = remain;
			dp->name_len = strlen(myname);
			strcpy(dp->name, myname);
			//Done. New entry in.
			put_block(pip->dev, pip->INODE.i_block[i], buff);
		}
		else
		{
			int bno = balloc(pip->dev);
			pip->INODE.i_size += BLKSIZE;
			get_block(pip->dev, bno, buff2);
			dp = (DIR *)buff2;
			cp = dp;
			dp->rec_len = BLKSIZE;
			dp->name_len = strlen(myname);
			dp->inode = myino;
			strcpy(dp->name, myname);
			put_block(pip->dev, bno, buff2);
		}

	}
}


#endif