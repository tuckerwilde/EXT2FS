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

      printf("Allocated:%d\n", i+1); 

      return i+1;
    }
  }
  printf("Balloc(): no more free inodes\n");
  return 0;
}

int mymkdir(MINODE *pip, char *name)
{

}


#endif