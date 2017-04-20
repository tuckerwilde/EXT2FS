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
	int tempIno, tempBno;
	MINODE *mip;
	char buff[BLKSIZE];
	char *cp;

	//Lets get some memory for these fools.
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
	printf("tempino: %d\n", tempIno);
	dp->rec_len = 12;
	dp->name_len = 1;
	strcpy(dp->name, ".");

	cp += dp->rec_len;
	dp = (DIR *)cp;

	//".."
	dp->inode = pip->ino;
	printf("pip->ino: %d\n",pip->ino);
	dp->rec_len = BLKSIZE-12;
	dp->name_len = 2;
	strcpy(dp->name, "..");

	//Write the data block back.
	put_block(dev, tempBno, buff);
	enter_name(pip, tempIno, name);


}

int enter_name(MINODE *pip, int myino, char *myname)
{
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

int my_creat(MINODE *pip, char *name)
{
	int tempIno, tempBno;
	MINODE *mip;
	char buff[BLKSIZE];
	char *cp;

	//Lets get some memory for these fools.
	tempIno = ialloc(dev);
	tempBno = balloc(dev);

	//Get that memory.
	mip = iget(dev, tempIno);

	//Temporary inode, let's fill out the new memory slot.
	INODE *tip = &mip->INODE;

	tip->i_mode = 0x81A4;
	tip->i_uid = running->uid;
	tip->i_gid = 0;
	tip->i_size = 0;
	tip->i_links_count = 1;
	tip->i_atime = tip->i_ctime = tip->i_mtime = time(0L); //this may break..
	tip->i_blocks = 2;
	tip->i_block[0] = tempBno;

	for (int i = 1; i < 15; i++)
	{
		tip->i_block[i] = 0;
	}

	mip->dirty = 1;
	iput(mip);
	enter_name(pip, tempIno, name);
}

int incFreeBlocks(int dev)
{
  char buff[BLKSIZE];

  get_block(dev, 1, buff);
  sp = (SUPER *)buff;
  sp->s_free_blocks_count++;
  put_block(dev, 1, buff);

  get_block(dev, 2, buff);
  gp = (GD *)buff;
  gp->bg_free_blocks_count++;
  put_block(dev, 2, buff);

}

int bdealloc(int dev, int bno)
{
	char buff[BLKSIZE];
	//Grab the bmap block
	get_block(dev, bmap, buff);
	//Clr the bit we would like
	clr_bit(buff, bno);
	//Increase the super and gd count
	incFreeBlocks(dev);
	//put it back, not in use anymore.
	put_block(dev, bmap, buff);
}

int incFreeInodes(int dev)
{
  char buff[BLKSIZE];

  // dec free inodes count in SUPER and GD
  get_block(dev, 1, buff);
  sp = (SUPER *)buff;
  sp->s_free_inodes_count++;
  put_block(dev, 1, buff);

  get_block(dev, 2, buff);
  gp = (GD *)buff;
  gp->bg_free_inodes_count++;
  put_block(dev, 2, buff);
}

int idealloc(int dev, int ino)
{
	char buff[BLKSIZE];
	//All same as bdealloc, but its the ino instead
	get_block(dev, imap, buff);
	clr_bit(buff, ino);
	incFreeInodes(dev);
	put_block(dev, imap, buff);
}

int rm_child(MINODE *pip, char *name)
{
	char buff[BLKSIZE], namebuff[BLKSIZE], removebuff[BLKSIZE];
	char *cp, *tempcp;
	int ideal_len, cur_len, prev_len, mem_to_move, run_total, temphold;

	//Deletion purposes
	for (int i = 0; i < BLKSIZE; i++)
	{
		removebuff[i] = 0;
	}
	for (int i = 0; i < 12; i++)
	{
		get_block(dev, pip->INODE.i_block[i], buff);
		dp = (DIR *)buff;
		cp = buff;
		tempcp = buff;

		//This is just tallying how far we've gone.
		run_total = 0;

		//Search for the name
		while (cp < &buff[BLKSIZE])
		{
			//Get the name into a buff
			strcpy(namebuff, dp->name);
			//Tricky null terminator
			namebuff[dp->name_len] = 0;
			//Tally the running total.
			run_total += dp->rec_len;
			cur_len = dp->rec_len;
			ideal_len = (4*((8+dp->name_len + 3)/4));

			if (strcmp(namebuff, name) == 0)
			{
				printf("Do we get here\n");
				if (dp->rec_len == BLKSIZE)
				{
					printf("First and Only.\n");
					bdealloc(dev, pip->INODE.i_block[i]);
					//Zero it out.
					pip->INODE.i_block[i] = 0;
					//Decrement parent
					pip->INODE.i_size -= BLKSIZE;
					//Move everything forward.
					int k = i;
					while (k < 12 && pip->INODE.i_block[k+1])
					{
						pip->INODE.i_block[k+1] = pip->INODE.i_block[k];
					}
					put_block(dev, pip->INODE.i_block[i], buff);
					return;
				}
				else if (dp->rec_len > ideal_len)
				{
					//Move the pointer back
					cp -= prev_len;
					dp = (DIR *)cp;
					dp->rec_len += cur_len;
				}
				else
				{
					//Could be a better way?
					int len = BLKSIZE - ((cp+cur_len) - buff);
					printf("Length %d\n", len);
					//This also is confusing. This is from moving
					//everything left from just past the found spot
					//all the way to the end of the block.
					memmove(cp, cp+cur_len, len);
					printf("Do we get here\n");
					//This looks confusing.. and it is...
					//Quick explanation: This goes until we overflow
					//with the current rec_len. Then adds it at the end.
					while (cp + dp->rec_len < &buff[1024] - cur_len)
					{
						printf("cp: %d buff: %d\n", cp, &buff[1024]);
						printf("Name: %s\n", dp->name);
						cp += dp->rec_len;
						dp = (DIR *)cp;
					}
					//Add the deleted length.
					dp->rec_len += cur_len;
				}
				put_block(dev, pip->INODE.i_block[i], buff);
				return;
			}
			prev_len = dp->rec_len;
			tempcp += dp->rec_len;
			cp += dp->rec_len;
			dp = (DIR *)cp;
		}
	}
	
}

int truncate(MINODE *mip)
{
	//TODO: Truncate indirect and double indirect.
	for (int i = 0; i < 12; i++)
	{
		if (mip->INODE.i_block[i] == 0)
			break;
		bdealloc(mip->dev, mip->INODE.i_block[i]);
	}
	mip->dirty = 1;
	mip->INODE.i_size = 0;
}

int myread(int temp_fd, char buff[], int nbytes)
{
	//nbytes is how many bytes to read.
	int count = 0;
	int left = 0;
	int avail, lbk, startByte, blk, blkhold, remain;

	char *cq = buff;
	char readbuff[BLKSIZE]; 
	char tempbuff[BLKSIZE];
	char tempbuff2[BLKSIZE];

	OFT *oftp;

	oftp = running->fd[temp_fd];

	avail = oftp->mptr->INODE.i_size - oftp->offset;

	while (nbytes && avail)
	{
		lbk = oftp->offset / BLKSIZE;
		printf("%d\n", lbk);

		startByte = oftp->offset % BLKSIZE;

		if (lbk < 12)
		{
			blk = oftp->mptr->INODE.i_block[lbk];
		}
		else if (lbk >= 12 && lbk < (256 + 12))
		{
			get_block(dev, oftp->mptr->INODE.i_block[12], tempbuff);
			blk = tempbuff[lbk-12];
		}
		else
		{
			get_block(dev, oftp->mptr->INODE.i_block[13],tempbuff2);
			blkhold = (lbk - (256+12))/256;

			get_block(dev, tempbuff2[blkhold], tempbuff);
			blk = tempbuff[(lbk - (256+12))%256];
		}
	

		get_block(oftp->mptr->dev, blk, readbuff);

		//This points at the exact spot in the block.
		char *cp = readbuff + startByte;
		//This is how much space there is in the block we just got.
		remain = BLKSIZE - startByte;
		//Lets move through this.
		//Three step algorithm!

		//Check first if we stop before end of block...
		if (nbytes <= remain) //We won't need to go back to outer loop.
			left = nbytes;
		else
			left = remain; //We go until end of block, get new block.
		//REAL QUICK overflow check!
		if (avail < left) //Essentailly we go over the whole spot..
			left = avail;

		//Great! Error checking out of the way.
		//Let's clear a buffer space.
		memset(cq, 0, BLKSIZE); //All of it.
		strncpy(cq, cp, left); //Go read comments above. Move stuff.

		//Updates.
		remain -= left; //Whats left in this block.
		nbytes -= left; //Whats left for us to read
		avail -= left; //Whats left in the whole entire group.
		count += left; //How many blocks we've read!
		oftp->offset += left;
		//Now to make it like cat...
		//Should be a string. If it breaks come back to this.
		printf("%s", cq); 
	}
	return count;
}

int mywrite(int temp_fd, char buff[], int nbytes)
{
	int lbk = 0, startByte = 0;
	OFT *oftp;

	oftp = running->fd[temp_fd];

	while (nbytes > 0)
	{
		lbk = oftp->offset/BLKSIZE;
		startByte = oftp->offset%BLKSIZE;

		if (lbk < 12)
		{
			
		}
	}
}

#endif