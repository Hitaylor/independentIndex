#ifndef _H_INDEX_TREE_H
#define _H_INDEX_TREE_H 
#include <linux/buffer_head.h>
#include "episode.h"
#include "timeIndex.h"
#include "tool.h"


enum {IDXDIRECT = 7, IDXDEPTH = 4};/*总共4级索引，7个直接，3个间接*/

typedef u32 indexblock_t;	/* 32 bit, host order */

static inline unsigned long index_block_to_cpu(indexblock_t n)
{
	return n;
}

static inline indexblock_t  cpu_to_index_block(unsigned long n)
{
	return n;
}

/**
 * 获取指向index数组的指针
 */
static inline indexblock_t *i_index(struct inode *inode)
{
	return (indexblock_t *)episode_i(inode)->i_index;
}


#define IDXDIRCOUNT 7 //i_index[10]中前7个元素用于直接寻址
#define IDXINDIRCOUNT(sb) (1<<((sb)->s_blocksize_bits-2))//1<<(12-2) = 1024,表示1个block中能存下多少个blockid
#define IDXCOUNTINBLOCK(sb) (1<<((sb)->s_blocksize_bits -TIMEINDEXLEN_BITS))//单个block内能存下的索引数量，这个4表示单条索引长度为2^4


/** 本函数用于寻找blockID这个逻辑块号存放的位置，其实就是对于给定的blockID，查找从i_index[]开始到该存储该blockID的位置的路径，
 * 这里的路径，就是从i_index[]开始怎么找到blockID这个u32的数值所在的位置，结果放在offset[]数组中，并返回从i_index[]开始需要几层查找。
 * 举个例子，最终存储内容的是房子。房子有一个全局唯一的房号（物理磁盘号），但是呢，房子是按排组织的，房子在这一排又有一个逻辑房号，
 * 每一排有排号（小区内的逻辑排号），排又是按小区组织的，小区有区号(街道内的逻辑小区号），小区又是按照街道组织的，
 * 对于间接寻址来说，就是拿到小区号，排号、房间号，这样才能得到最终的物理房号。offset[]记录的就是这些逻辑号。
 * 因为采用i_index[10]进行管理索引存放的位置，而i_index[10]又采用了
 * 一级、二级和三级间接3种途径来增加管理的block数量，所以在对blockID 对应的block进行读写之前，需要先找到它对应的真实的物
 * 理磁盘上的块号。这是因为逻辑块号是连续的，但其对应的真实物理磁盘块号不一定连续，所以要先找到逻辑块号blockID 对应的真实
 * 物理块号是多少，而这些物理块号又采用了i_index[10]进行管理。本函数只是计算出逻辑块号blockID在i_index所管理的4层寻址方案
*  用到了几层（depth），以及在每一层的偏移量offset，是用一级间接寻址，还是二级间接选址，还是三级，在这些寻址方案中，每一级
*  的偏移量是多少。也就是说，本函数只是算术上的计算，不涉及读磁盘的操作。
 * 如果blockID=10,则因为一级寻址1-1024个block，明显包括第10个block，所以offset[0]=0,offset[1]=10,n=2;
 * 
 * 再假如blockID =2200,则显然要用到了二级间接，同样的，offset[0]=1表示用到了二级间接寻址，由于二级间接是从第1025个开始的，所
 * 以还剩下2200-1025=1175个block是由二级间接管理的，我们知道二级间接包含两级，i_index[2]指向的block存储的是1024个blockID,
 * 这1024个blockID中的每一个又对应了1个block，而这个block里面存储的又是blockID.
 * @param inode,
 * @param blockID, 存储索引数据的逻辑块号（不是物理磁盘上的块号）,也就是存储inode对应的文件索引数据的第blockID 个block
 * @param offset, 用于存储寻找@blockID 这个id的block的路径，（由i_index[10]管理），offset[0]的值为i_index[10]的某个下标，
 */
static int index_block_to_path(struct inode * inode, long blockID, int offsets[IDXDEPTH]){
	int n = 0;
	struct super_block *sb = inode->i_sb;
	int tmp = 0;
	if (blockID < 0) {//文件的逻辑块号blockID从0开始计算
		printk("episode-fs:[indextree.h] index_block_to_path: blockID %ld < 0 on dev %pg\n", blockID, sb->s_bdev);
	} else if ((u64)blockID * (u64)sb->s_blocksize >= episode_sb(sb)->s_max_size) {//索引数据也不能太长了
		if (printk_ratelimit())
			printk("episode-fs: [indextree.h] block_to_path: block %ld too big on dev %pg\n", blockID, sb->s_bdev);
	} else if (blockID < IDXDIRCOUNT) {//直接寻址
		offsets[n++] = blockID;
	} else if ((blockID -= IDXDIRCOUNT) < IDXINDIRCOUNT(sb)) {//一级间接寻址
		offsets[n++] = IDXDIRCOUNT;//offset[0] =7也就是一级间接寻址块或者说代表i_index[7]
		offsets[n++] = blockID;//从0开始计数,i_index[7]的数值代表的那个block当作u32的数组，offset[1]存储的就是数组下标
	} else if ((blockID -= IDXINDIRCOUNT(sb)) < IDXINDIRCOUNT(sb) * IDXINDIRCOUNT(sb)) {//二级间接寻址
		offsets[n++] = IDXDIRCOUNT + 1;//offset[0]=8
		offsets[n++] = blockID / IDXINDIRCOUNT(sb);//i_index[7]的数值代表的那个block当作u32的数组bid1[1024]，offset[1]存储的就是数组下标k
		offsets[n++] = blockID % IDXINDIRCOUNT(sb);//bid1[k]的数值所代表的那个block当作u32的数组bid2[1024]，offset[2]存储的就是数组下标t，
							   //而bid2[t]的数值表示的block就是最终存储索引数据的磁盘块
	} else {//三级间接寻址
		blockID -= IDXINDIRCOUNT(sb) * IDXINDIRCOUNT(sb);
		offsets[n++] = IDXDIRCOUNT + 2;//offset[0]=9
		offsets[n++] = (blockID / IDXINDIRCOUNT(sb)) / IDXINDIRCOUNT(sb);
		offsets[n++] = (blockID / IDXINDIRCOUNT(sb)) % IDXINDIRCOUNT(sb);
		offsets[n++] = blockID % IDXINDIRCOUNT(sb);
	}
	printk("[indextree.h index_block_to_path()] depth=%d",n);
	for(tmp = 0; tmp < n; tmp++) printk("offsets[%d]=%d",tmp,offsets[tmp]);
	return n;
}

typedef struct {
	indexblock_t	*p;//指向本级记录块号映射表中的表项目，将一个block当成存储blockid的u32数组bid[1024]，p指向某一个元素k，也就是p=&bid[k]
	indexblock_t	key;//p表示的表项的内容,就是blockid，它和p是一一对应的，对于p=&bid[k]的情况下，key=bid[k]，但这个需要用户设定p和key的关系，默认是没关系的，所以，splice_index_branch的主要功能就是修复p和key的不一致。
	struct buffer_head *bh;//缓冲区头部
} IDXindirect; //索引的间接寻址结构体

static DEFINE_RWLOCK(index_pointers_lock);//索引的元数据的读写锁


//v表示指向一个blockid的指针，bh表示v代表的block在page cache中对应的缓冲区头;这个函数的作用是构建一个branch中的一个孤立节点，
static inline void add_index_chain(IDXindirect *p, struct buffer_head *bh, indexblock_t *v)
{
	p->key = *(p->p = v);//假如i_index[5]=51,则p->key=5,p->p=&i_index[5]。这里就是设定p和key的关系
	p->bh = bh;
}


/**
 * 验证链表中的表项是否正确，也就是IDXindirct中的p指针指向的内容是否和key相同。正常情况下，while之后的指针from是大于指针to的
 */
static inline int verify_index_chain(IDXindirect *from, IDXindirect *to)
{
	printk("verify: from=%p,to=%p",from,to);
	while (from <= to && from->key == *from->p)
		from++;
	return (from > to);
}

/**
 * 返回bh的数据区的终止位置的指针
 */
static inline indexblock_t * index_block_end(struct buffer_head *bh)
{
	return (indexblock_t *)((char*)bh->b_data + bh->b_size);
}




/**
 * 前面index_block_to_path()是纯数学的逻辑计算，给定blockid，得出需要几层查询，在每层中的偏移量，举例子说，这个路径是2层的，第一层从第7个房间拿钥匙，第二层从第9个房间 *  拿钥匙。但index_block_to_path()得到的只是应当去哪里找最终的blockid，但这些应当对应的位置可能还没有数值（也就是key=0），第7个房间里可能有钥匙，也可能没有钥匙;如果没* 有钥匙，则要先分配一个钥匙，表明是哪一排。get_index_branch就是根据offset[]给出的路径，去读block，然后根据偏移量（数组下标）对应的blockid一层层读下去。如果一路读下来* 没问题，就可以得到最终的blockid就是用于存储索引数据的物理块的块号，此时得到一个完整的chain，函数返回NULL;如果在读的过程中，某个IDXindirect节点的key=0,则往下没法读
* 了，此时得到的chain是不完整的，后面一节是缺失的，函数返回chain的最后一个IDXindirect对象的指针，该指针的bh是有内容的，p也是有内容的，但是key为0。后面需要调用
*  alloc_index_branch()把chain后面的部分补起来，但是它只是把后面的那一段连起来，并没有把它和前面的chain连起来。这个断链的修复需要splice_index_branch()来完成。
* （因为0号block不存在，文件系统的物理块号是从1开始的（被根节点占用了），另外这里的物理块号是从superblcok中first_block开始计算的，之前的用于存储sb、imap、zmap，inodetable的都不算在内。)
 * chain用来存储/表示branch，chain[0]对应offset[0]也就是i_index[]中的某一个值，因为可以直接拿到blockid，所以不需要读盘，所以没有bh;
 * chain[1]对应一级间接寻址，表示offset[0]对应的那个block中存储的某个blockid，这个是需要读盘获取offset[0]记录的blockid对应的block的，
 * 所以有bh; chain[2]和chain[3]类似。
 */
static inline IDXindirect *get_index_branch(struct inode *inode,
					int depth,
					int *offsets,
					IDXindirect chain[IDXDEPTH],
					int *err)
{
	struct super_block *sb = inode->i_sb;
	IDXindirect *p = chain;//初始化p指向链表头
	struct buffer_head *bh;
	*err = 0;
	/* i_index是固定长度的,把i_index[offset[0]]这个blockid对应的block信息加入到chain的头部 */
	add_index_chain(chain, NULL, i_index(inode) + *offsets);//i_index函数返回(indexblock_t *)episode_i(inode)->index，就是指向i_index[10]的指针，*offsets应该是0-9的数值，表示为i_index[]的下标，表示对应的blockid由哪个块寻址
	//将offset[0]加到chain上作为第一个元素，因为是直接寻址，所以没有bh（也就是0<=offset[0]<10的内容就是一个blockid）
	printk("[get_index_branch()] offsets[0]=%d",*offsets);
	if (!p->key){//说明是第一次写入，i_index[0]=0
			printk("This is the first time writing index to the block, so offset[0]=0, there has not assigned a block for the index content! ");
			goto no_block;
		}
//如果key=0,则说明前面的i_index[offset[0]-1]所管理的那些block都写满了（offset[0]>0时）;或者是第一条索引内容（offset[0]=0)。此时，相当于第一层都是缺失的，那整个链表相当于啥也没有，整个链条都要重建！此时返回p，p为&i_index[offset[0]]
	//可以确定的是，只要中间某层的key为0,则下面的key都为0，但反过来并不成立，所以，这里采用从最下一层往上追溯的方式来确定在哪些层是key为0的，到哪层开始key不是0;key=0,则表示chain在此处断开了，需要后面修补。
	while (--depth) {//用到了几级寻址，depth>=2才会到这里
		//这个需要确认是否真的读磁盘了，还是说先查缓冲区？
		bh = sb_bread(sb, index_block_to_cpu(p->key));//读取i_index[10]涉及到的存储blockid的块到缓冲区，并用bh指向它。
		//sb_bread()只要不是IO错误，都会返回一个bh，如果p->key代表的block不是新建的，则先查buffercache，找到则返回，没找到则读盘;如果是对应的block为空，则从buffercache中申请一个bh。也就是只要不是发生了IO 错误，sb_bread()都会返回一个可用的bh。
		if (!bh)
			goto failure;
		read_lock(&index_pointers_lock);
		if (!verify_index_chain(chain, p))//因为p是上一步加进来的，是chain的最后一个节点，正常情况下chain是连续的，也就是verify_index_chain()返回true;如果返回0,说明chain断掉了，或者更新了，
			goto changed;
		//如果chain中是正常的，则从新读的block的内容创建一个新的IDXindirect节点，并加到chain的尾部
		add_index_chain(++p, bh, (indexblock_t *)bh->b_data + *++offsets);//把offset数组其余元素链进来，bh表示一个缓冲区对应的缓冲头，缓冲区存的就是一个block的内容，也就是blockid;第三个参数就是指向这个block中的某一条blockid的指针
		read_unlock(&index_pointers_lock);
		if (!p->key)
			goto no_block;//前面已经把新建的节点加入到了chain中，如果最后一个节点指向的位置的内容为0（也就是p有值，但key为0），则说明chain在此处断了，chain的最后一个元素的key=0,后面即使从p处开始alloc_index_branch()，也只是把p之后的部分重建了一个chain2,但是chain的最后一个节点的key还没有修复，它应该等于chain2的首节点对应的blockid放到chain的末尾节点的key字段中，这样chain和chian2才修复好了
	}
	return NULL;//正常情况下返回NULL 

changed:
	read_unlock(&index_pointers_lock);
	brelse(bh);
	*err = -EAGAIN;
	goto no_block;
failure:
	*err = -EIO;
no_block:
	printk("[indextree.h get_index_branch()]p=%p, p->p=%p,p->key=%d",p,p->p,p->key);
	return p;
}


/**
 *前面get_index_branch()已经说过了，如果返回的不是NULL，则说明chain断了，返回的是断裂出的p。为了修复chain，需要构建p之后的部分，假设为chain2。在get_index_branch()注释中提到了，只要p处断了，说明key=0,则下面的各层的key=0,说明都还没有分配block，则p后面有几层，就需要申请几个block。
一般来说，只要get_index_branch()!=NULL，则后面就会将p作为参数，调用alloc_index_branch()构建chain2,从本函数可知，p的key有值了，就是新申请的block的id，下面各层的key和p都有数值了，且下面各层的p都指向了存储key的位置，唯独p的key和p的关系还没有修复。所以本函数返回之后，一般就要调用splice_index_branch()来修复断裂处的p和key的关系。
 * return       0, 成功
          -ENOSPC，失败
 */ 
static int alloc_index_branch(struct inode *inode,
			     int num,
			     int *offsets,
			     IDXindirect *branch)
{
	int n = 0;
	int i;
	int blockID = episode_new_block(inode);//申请到的blockID，此时只是一个数字而已，还灭有在缓冲区分配空间。（本文件系统的启动区占据的blockid为0,sb占据的blockid为1）
	printk("the new blockid = %d in alloc_index_branch()!");
	branch[0].key = cpu_to_index_block(blockID);//brach[0].key=blockID,brach[0].p应该就是i_index[]中的某个元素的地址了，但是这里并没有给p赋值，这就造成了i_index[]和branch是灭有连起来的，这部分工作交给了splice_index_branch()来完成
		//branch[0]不需要有bh，因为它对应的blockid是从i_index[10]中直接获取的
	if (blockID) for (n = 1; n < num; n++) {
		struct buffer_head *bh;
		/* Allocate the next block */
		int subBlockID = episode_new_block(inode);//根据num的数值（0-3或者1-4），为branch[1] branch[2] branch[3]申请blockid
		if (!subBlockID)
			break;
		branch[n].key = cpu_to_index_block(subBlockID);
		bh = sb_getblk(inode->i_sb, blockID);//在内存的缓冲区为blockID代表的磁盘块分配一个缓冲区，并将对应的bh返回。这里因为是新分配的block，block中无内容，所以sb_getblk()先从buffercache中查找，如果有则返回，如果没有则创建一个buffer（page）并返回，并不读磁盘。sb_bread()里面相当于先调用了sb_getblk(),然后判断数据是不是最新的，如果不是最新的，则读盘。这两者的区别就是sb_bread()比sb_getblk()多了一个数据有效性的判断和处理步骤。对于新分配的bh，数据肯定是有效的（因为灭有数据），所以可以不判断。
		lock_buffer(bh);
		memset(bh->b_data, 0, bh->b_size);//把新申请的缓冲区清零
		branch[n].bh = bh;//bh绑定到branch中的对应元素上
		branch[n].p = (indexblock_t*) bh->b_data + offsets[n];//指向缓冲区内的某个位置，这个位置的内容就是前面申请到的subBlockID
		*branch[n].p = branch[n].key;
		set_buffer_uptodate(bh);//重点关注buffer的相关操作
		unlock_buffer(bh);
		mark_buffer_dirty_inode(bh, inode);//这个不是page哦，而是缓冲区buffer，对应到inode中，是i_index[]相关结构以及时间发生了变化
		blockID = subBlockID;
	}
	if (n == num){
		return 0; //分配成功则返回0.
	}
		

	/* Allocation failed, free what we already allocated */
	for (i = 1; i < n; i++)
		bforget(branch[i].bh);
	for (i = 0; i < n; i++)
		episode_free_block(inode, index_block_to_cpu(branch[i].key));
	return -ENOSPC;
}


/**
 * 拼接branch，主要目的是将where连到chain上。主要做了两件事：（1）verify_index_chain()确认chain和where这两条链条属于同一链条，防止移花接木;（2）修复where的首节点的p和key的关系。因为实际修复的只有where的首节点，所以原则上讲，参数只需要where就够了，这里之所以吧chain放上去，就是要确认chain和where属于同一链条。
 */ 
static inline int splice_index_branch(struct inode *inode,
				     IDXindirect chain[IDXDEPTH],
				     IDXindirect *where,
				     int num)
{
	int i;

	write_lock(&index_pointers_lock);
	printk("partial==chain!chain=%p,where-1 =%p,where=%p,where->p=%p,*where->p=%d",chain,where-1,where,where->p,*where->p);
	/* 确认一下要链接的地方是不是还在那里，且没有发生变化 */
	printk("verify_index_chain(chain, where-1)=%d",verify_index_chain(chain, where-1));
	if (!verify_index_chain(chain, where-1) || *where->p)//判断chain中在where之前的各项是否正常。
		{
			printk("I am here!");
			goto changed;
		}
	printk("I am here 2!");
	*where->p = where->key;//这就是这个函数唯一做的事情，就是把where的key放到了p指向的位置。
	printk("in the splice, where->p=%p,*where->p=%d",where->p,*where->p);
	write_unlock(&index_pointers_lock);

	/* We are done with atomic stuff, now do the rest of housekeeping */

	inode->i_ctime = current_time(inode);//为什么修改的是创建时间？

	/* had we spliced it onto indirect block? */
	if (where->bh)
		mark_buffer_dirty_inode(where->bh, inode);//标记where的bh发生了变化

	mark_inode_dirty(inode);//标记inode发生了变化
	return 0;

changed:
	write_unlock(&index_pointers_lock);
	for (i = 1; i < num; i++)
		bforget(where[i].bh);
	for (i = 0; i < num; i++)
		episode_free_block(inode, index_block_to_cpu(where[i].key));
	return -EAGAIN;
}

static struct buffer_head * get_index_block_bh_for_write(struct inode * inode, sector_t block, int err)
{
	struct buffer_head * bh;
	err = -EIO;
	int offsets[IDXDEPTH];
	IDXindirect chain[IDXDEPTH];
	IDXindirect *partial;
	int left;
	int depth = index_block_to_path(inode, block, offsets);//填充offset[],获取层数depth
	printk(" get_index_block_bh_for_write()bh =%p,&bh=%p",bh,&bh);
	if (depth == 0)//depth必须大于0,至少要有1层（对应直接寻址）。
		goto failure;
	printk("[indextree.h get_index_block_for_write()] before call the get_index_branch, the partial =%p",partial);

reread:
	partial = get_index_branch(inode, depth, offsets, chain, &err);//根据depth和offset来填充chain，chain可能是完整的，也可能是前半段，是哪种情况，由partial标识
	//如果顺利完成映射，则返回NULL;否则肯定是IDXindirect中某一项的key为0，说明这一项（记录块）原来不存在，也就是下一层的block是不存在的，现在因为新增加了索引数据，导致需要扩展索引数据的长度，就需要新的block了
	//printk("[indextree.h get_index_block_for_write()]&chain[0]=%p, chain[0]->p=%p,chain[0]->key=%d,partial=%p",&chain[0],chain[0].p,chain[0].key,partial);
	if(partial == NULL){
		//对于首次写之前，chain[0].key=0，这个不成立
		printk("[indextree.h get_index_block_for_write()]partial=NULL");
	}

	/* 最简单的情况 - chain是完整的，最终的存储索引数据的block找到了（chain的最后一个节点的key的数值就是文件系统中的物理块的id）,不需要申请新block */
	if (!partial) {//partial为NULL,说明chain[depth-1].key对应的物理块已经有部分索引数据了，但还没有写满，需要读盘sb_bread()
		//printk("[indextree.h get_index_block_for_write()] partial =NULL, chain[depth-1].key=%d!",index_block_to_cpu(chain[depth-1].key));
		bh = sb_bread(inode->i_sb,index_block_to_cpu(chain[depth-1].key));
		printk("after sb_bread(), bh=%p,&bh=%p",bh,&bh);
		if (!bh){
			printk("[indextree.h get_index_block_for_write()]bh=NULL");
			err = -EIO;
			partial = chain+depth-1;
			while(partial >chain){
				brelse(partial->bh);
				partial--;			
			}
 		}
		 else{
			 printk("[indextree.h get_index_block_for_write()] bh got by sb_bread() is not NULL, and the correspoding blocknr=%d,bh=%p,&bh=%p",bh->b_blocknr,bh,&bh);
		 }
		
		set_buffer_uptodate(bh);
		printk("bh=%p,&bh=%p",bh,&bh);
		return bh;
/*got_it:
		//这一步相当于找到了最终存储数据的block，对于i_zone[10]来说，数据由page cache管理，所以需要将block和pagecache关联起来;对于我们的i_index{10],我们是要将block使用sb_bread(sb,block,block_size)读到内存中，同时返回对应的bh（也就是要修改函数的返回值为buffer_head * )
		map_bh(bh, inode->i_sb, index_block_to_cpu(chain[depth-1].key));//linux/buffer_head.h，将块号为第三个参数的block和bh这个缓冲头绑定，主要就是设定bh的一些字段，让bh表示的缓冲区就是第三个参数代表的那个block。但是bh是有page_buffer(page)得到的，也就是说，这里相当于是把pgae 和buffer绑定，这一步很可能只是文件内容放到page cache的buffer中所需要的，我们应该不需要这个。
		// Clean up and exit 
		partial = chain+depth-1; //the whole chain, partial指向chain中最后一个节点，然后逐个释放掉整条chain的bh和bh->data指向的buffercache，也就是释放掉i_zone[10]管理的那些存储blockid的元数据。
		goto cleanup;*/
	}
	else{//chain不是完整的

		left = (chain + depth) - partial;//chain和partial都是u32指针，depth也是u32类型的数值，left也是u32,所以直接相减，得到的就是断开处之后的子链的长度，也就是需要alloc从链表的长度
		err = alloc_index_branch(inode, left, offsets+(partial-chain), partial);
		printk("err=%d,partial=%p,partial->key=%d,partial->p=%p",err,partial,partial->key,partial->p);
		if (err){
			partial = chain+depth-1; 
			while(partial >chain){
				brelse(partial->bh);
				partial--;			
			}
			return NULL;
		}

		if (splice_index_branch(inode, chain, partial, left) < 0)
		{
			printk("splice the index branch failed!");
			goto changed;
		}
	}
changed:
	printk("partial=%p,chain=%p",partial,chain);
	while (partial > chain) {
		brelse(partial->bh);
		partial--;
	}
	goto reread;

failure:
	err = -EIO;
	return NULL;
}

/**
 * 本函数的目标是根据存储索引数据的逻辑块号@block （根据当前索引数据的指针计算得到），为@inode对应的文件在块高速缓冲区申请一个块缓存，并与磁盘上的对应物理块保持一致
 * @param inode, 待分配block的文件对应的inode，或者说准备写索引的文件对应的inode
 * @param block, 记录索引数据写入位置的游标所在的逻辑块号
 * @param bh, 从块高速缓冲区中申请的与逻辑块block对应的物理块phyblk对应的缓冲块的头部
 * 
 *
 */
static int get_index_block_for_write(struct inode * inode, sector_t block,
			struct buffer_head *bh)
{
	int err = -EIO;
	int offsets[IDXDEPTH];
	IDXindirect chain[IDXDEPTH];
	IDXindirect *partial;
	int left;
	int depth = index_block_to_path(inode, block, offsets);//填充offset[],获取层数depth
	printk("[indextree.h get_index_block_for_write()] depth=%d!*bh =%p,bh=%p",depth,*bh,bh);
	if (depth == 0)//depth必须大于0,至少要有1层（对应直接寻址）。
		goto failure;
	printk("[indextree.h get_index_block_for_write()] before call the get_index_branch, the partial =%p",partial);

reread:
	partial = get_index_branch(inode, depth, offsets, chain, &err);//根据depth和offset来填充chain，chain可能是完整的，也可能是前半段，是哪种情况，由partial标识
	//如果顺利完成映射，则返回NULL;否则肯定是IDXindirect中某一项的key为0，说明这一项（记录块）原来不存在，也就是下一层的block是不存在的，现在因为新增加了索引数据，导致需要扩展索引数据的长度，就需要新的block了
	printk("[indextree.h get_index_block_for_write()]&chain[0]=%p, chain[0]->p=%p,chain[0]->key=%d,partial=%p",&chain[0],chain[0].p,chain[0].key,partial);
	if(partial == NULL){
		//对于首次写之前，chain[0].key=0，这个不成立
		printk("[indextree.h get_index_block_for_write()]partial=NULL");
	}
	/* 最简单的情况 - chain是完整的，最终的存储索引数据的block找到了（chain的最后一个节点的key的数值就是文件系统中的物理块的id）,不需要申请新block */
	if (!partial) {//partial为NULL,说明chain[depth-1].key对应的物理块已经有部分索引数据了，但还没有写满，需要读盘sb_bread()
		printk("[indextree.h get_index_block_for_write()] partial =NULL, chain[depth-1].key=%d!",index_block_to_cpu(chain[depth-1].key));
		bh = sb_bread(inode->i_sb,index_block_to_cpu(chain[depth-1].key));
		if (!(bh)){
			printk("[indextree.h get_index_block_for_write()]bh=NULL");
			err = -EIO;
			partial = chain+depth-1;
			while(partial >chain){
				brelse(partial->bh);
				partial--;			
			}
 		}
		 else{
			 printk("[indextree.h get_index_block_for_write()] bh got by \
			 sb_bread() is not NULL, and the correspoding blocknr=%d,bh=%p,&bh=%p",\
			 (bh)->b_blocknr,bh,&bh);
		 }
		
		set_buffer_uptodate(bh);
		printk("bh=%p, &bh=%p",bh,&bh);

		return err;
/*got_it:
		//这一步相当于找到了最终存储数据的block，对于i_zone[10]来说，数据由page cache管理，所以需要将block和pagecache关联起来;对于我们的i_index{10],我们是要将block使用sb_bread(sb,block,block_size)读到内存中，同时返回对应的bh（也就是要修改函数的返回值为buffer_head * )
		map_bh(bh, inode->i_sb, index_block_to_cpu(chain[depth-1].key));//linux/buffer_head.h，将块号为第三个参数的block和bh这个缓冲头绑定，主要就是设定bh的一些字段，让bh表示的缓冲区就是第三个参数代表的那个block。但是bh是有page_buffer(page)得到的，也就是说，这里相当于是把pgae 和buffer绑定，这一步很可能只是文件内容放到page cache的buffer中所需要的，我们应该不需要这个。
		// Clean up and exit 
		partial = chain+depth-1; //the whole chain, partial指向chain中最后一个节点，然后逐个释放掉整条chain的bh和bh->data指向的buffercache，也就是释放掉i_zone[10]管理的那些存储blockid的元数据。
		goto cleanup;*/
	}
	else{//chain不是完整的

		left = (chain + depth) - partial;//chain和partial都是u32指针，depth也是u32类型的数值，left也是u32,所以直接相减，得到的就是断开处之后的子链的长度，也就是需要alloc从链表的长度
		err = alloc_index_branch(inode, left, offsets+(partial-chain), partial);
		printk("err=%d,partial=%p,partial->key=%d,partial->p=%p",err,partial,partial->key,partial->p);
		if (err){
			partial = chain+depth-1; 
			while(partial >chain){
				brelse(partial->bh);
				partial--;			
			}
			return err;
		}

		if (splice_index_branch(inode, chain, partial, left) < 0)
		{
			printk("splice the index branch failed!");
			goto changed;
		}
	}
changed:
	printk("partial=%p,chain=%p",partial,chain);
	while (partial > chain) {
		brelse(partial->bh);
		partial--;
	}
	goto reread;

failure:
	err = -EIO;
	return err;
}

/**判断从p到q的内容是不是都是0,如果都是0,返回1,否则返回0，这个主要是用于判断bh指向的缓冲区到q之前的位置是否为0
 */
static inline int index_all_zeroes(indexblock_t *p, indexblock_t *q)
{
	while (p < q)
		if (*p++)//如果p不是0
			return 0;
	return 1;
}


/**
 * 这个函数是给truncate()用的，主要的作用是找到要截断的块所在的枝干
 */ 

static IDXindirect *find_index_shared(struct inode *inode,
				int depth,
				int offsets[IDXDEPTH],
				IDXindirect chain[IDXDEPTH],
				indexblock_t *top)
{
	IDXindirect *partial, *p;
	int k, err;

	*top = 0;
//offsets[]={9,0,2,3}，for循环之后，k=4;offsets[]={9,1,0,0}循环之后，k=2
	for (k = depth; k > 1 && !offsets[k-1]; k--)//找到offset[]中最后一个不是0（也就是找offsets中从尾部开始的连续0的起始位置）,如果都不为0,则k=depth;否则，k停在最后一个0的位置。offsets从depth-1到1为止，也就是只涉及间接寻址，不涉及到直接寻址
		;
	partial = get_index_branch(inode, k, offsets, chain, &err);//根据offsets[]找前k层，k可能=depth，也可能是offsets[]最后的连续0的开始位置。
	//offsets[]中是否有0,跟chain中节点key是否为0没关系;
	write_lock(&index_pointers_lock);
	if (!partial)
		partial = chain + k-1;
	if (!partial->key && *partial->p) {//key=0,断链，但是断链处的*partial->p不是为key吗？
		write_unlock(&index_pointers_lock);
		goto no_top;
	}
	for (p=partial;p>chain && index_all_zeroes((indexblock_t*)p->bh->b_data,p->p);p--)//什么也不做？
		;
	if (p == chain + k - 1 && p > chain) {
		p->p--;
	} else {
		*top = *p->p;
		*p->p = 0;
	}
	write_unlock(&index_pointers_lock);

	while(partial > p)
	{
		brelse(partial->bh);
		partial--;
	}
no_top:
	return partial;
}

static inline void free_index_data(struct inode *inode, indexblock_t *p, indexblock_t *q)
{
	unsigned long nr;

	for ( ; p < q ; p++) {
		nr = index_block_to_cpu(*p);
		if (nr) {
			*p = 0;
			episode_free_block(inode, nr);
		}
	}
}

/**
 * 递归调用函数
 * @param p, block id
 */ 
static void free_index_branches(struct inode *inode, indexblock_t *p, indexblock_t *q, int depth)
{
	struct buffer_head * bh;
	unsigned long nr;

	if (depth--) {
		for ( ; p < q ; p++) {
			nr = index_block_to_cpu(*p);
			if (!nr)//id 为0的block
				continue;
			*p = 0;
			bh = sb_bread(inode->i_sb, nr);
			if (!bh)
				continue;
			free_index_branches(inode, (indexblock_t*)bh->b_data,
				      index_block_end(bh), depth);
			bforget(bh);
			episode_free_block(inode, nr);
			mark_inode_dirty(inode);
		}
	} else
		free_index_data(inode, p, q);
}

/****
 * 这个函数主要处理如何截断文件文件数据，解决如何删除那些我们不需要的数据块的问题。所谓的截断文件指的是：一个特定长度的文件，我们从某个位置开始丢弃后面的数据，之前的数据依然保留。对具体文件系统来说，截断数据主要意味着两件事情：1. 文件大小发生变化；2. 文件被截断部分之前占用的数据块（包括存储blockid的那些i_zone[]管理的元数据块）释放，让其他文件可以使用。对于我们的索引系统来说，应该还要删除i_index管理的那些block。这个函数放到itree.h中，不要放在这里了。另外，这个函数本身没有带什么注释，所以我当初理解的时候浪费了很多时间。这里，很让人迷惑的是iblock的计算方法，它使用的是inode->i_size的位置为起点计算要删除的block的起点。因为我们的理解是inode中的i_size字段表示的是文件当前的长度，也就是文件的终止位置，如果从这个地方开始截断，那不相当于什么都不做吗？如果将本函数理解为删除所有的数据，那后面 根据n的数值进行删除的操作又有问题，比如n=1时，它删除的是idata+offset[0]--idata+DIRECT所管理的那些block。后来，经过不断探索，我发现，不能把i_size当成文件的真实长度，而是在文件写入失败或者文件长度超过限制的时候，我们是要先修改i_size的，将之设定为预期的长度，然后调用truncate()将超出预期长度的内容及对应的元数据删掉。其实这个函数让人难以理解是因为它实际上将需要截断的位置这个参数给隐藏掉了，如果再加1个截断位置的变了pos，来替换inode—>i_size，估计就容易理解了。
 */ 
/*
static inline void truncate (struct inode * inode)
{
	struct super_block *sb = inode->i_sb;
	indexblock_t *idata = i_data(inode);

	int offsets[DEPTH];
	Indirect chain[DEPTH];
	Indirect *partial;
	indexblock_t nr = 0;
	int n;
	int first_whole;
	long iblock;
	//这里不要
	iblock = (inode->i_size + sb->s_blocksize -1) >> sb->s_blocksize_bits;//inode数据占用多少个block
	block_truncate_page(inode->i_mapping, inode->i_size, get_block);//这一步应该是删除page cache中的内容（根据基树进行查找，然后清理）

//后面应该是删除i_zone管理的那些block，也就是元数据占用的block
	n = block_to_path(inode, iblock, offsets);//找到截断开始的第一个数据块的路径，填写到offsets[]，则offsets[]记录的偏移量为需要删除的文件内容的起始位置，如offsets[]={9,1,10,2}则从{9,1,10,2}表示的block开始，之后的这些存储数据的block以及存储这些blockid的block，都需要清理掉
	if (!n)
		return;
	//如果截断的起始位置位于直接寻址区
	if (n == 1) {//先处理直接寻址管理的那部分元数据
		free_data(inode, idata+offsets[0], idata + DIRECT);//idata[offset[0]]--idata[6]这一部分因为blockid只存在inode的idata中，不需要磁盘上的block存储，所以这一部分释放，只需要释放它们管理的(7-offset[0])个数据块即可，也就是这一部分的元数据（映射块不占磁盘上的具体block）
		first_whole = 0;
		goto do_indirects;//再处理间接寻址管理的那些元数据
	}

	first_whole = offsets[0] + 1 - DIRECT;
	partial = find_shared(inode, n, offsets, chain, &nr);//找到要截断的块所在的枝干
	if (nr) {
		if (partial == chain)
			mark_inode_dirty(inode);
		else
			mark_buffer_dirty_inode(partial->bh, inode);
		free_branches(inode, &nr, &nr+1, (chain+n-1) - partial);
	}
	// 释放共享的枝干上的间接块 
	while (partial > chain) {
		free_branches(inode, partial->p + 1, block_end(partial->bh),
				(chain+n-1) - partial);
		mark_buffer_dirty_inode(partial->bh, inode);
		brelse (partial->bh);
		partial--;
	}
do_indirects:
	// 开始释放间接映射块，因为first_whole表示用到的寻址层级，肯定 
	while (first_whole < DEPTH-1) {
		nr = idata[DIRECT+first_whole];//
		if (nr) {
			idata[DIRECT+first_whole] = 0;//释放inode的i_data[]
			mark_inode_dirty(inode);
			free_branches(inode, &nr, &nr+1, first_whole+1);//将i_data[nr]管理的所有block释放掉。
		}
		first_whole++;
	}
//增加删除i_index所管理的索引数据，以及间接映射块，但第一步是查找截断处的位置在i_index[]中的位置，然后才能确定i_index[]管理的索引数据的截断位置，然后才能删除

	inode->i_mtime = inode->i_ctime = current_time(inode);
	mark_inode_dirty(inode);
}
*/
/**
 * 这个函数不一定需要，要确认truncate函数是不是删除文件的时候才会用到，这个函数的目标应该是根据inode的i_zone管理的那些blockid，
 * 然后删除page cache中的各个page，并不是删除i_zone管理的那些个存储blockid的block。当然，在删除文件的时候，不仅仅要删除i_zone
 * 管理的那些block最终指向的存储文件内容的那些block，也要清退i_zone和i_index管理的那些存储blockid的block
 */ 
static inline void truncate_index (struct inode * inode, indexblock_t timeIndexPos)
{
	struct super_block *sb = inode->i_sb;
	struct episode_inode_info *episode_inode = episode_i(inode);
	indexblock_t *i_index = episode_inode->i_index;
		int offsets[IDXDEPTH];
	IDXindirect chain[IDXDEPTH];
	IDXindirect *partial;
	indexblock_t nr = 0, indexNum = 0, indexSize = 0;
	int n;
	int first_whole;
	long iblock;//i_index总共管理多少个block，也就是索引数据总共占用了多少个block
	//indexNum = episode_inode->i_indexnum;
	//indexSize = indexNum*TIMEINDEXLEN;

	iblock = (timeIndexPos + sb->s_blocksize -1) >> sb->s_blocksize_bits;

	n = index_block_to_path(inode, iblock, offsets);
	if (!n)
		return;

	if (n == 1) {
		free_index_data(inode, i_index+offsets[0], i_index + IDXDIRECT);
		first_whole = 0;
		goto do_indirects;
	}

	first_whole = offsets[0] + 1 - IDXDIRECT;
	partial = find_index_shared(inode, n, offsets, chain, &nr);
	if (nr) {
		if (partial == chain)
			mark_inode_dirty(inode);
		else
			mark_buffer_dirty_inode(partial->bh, inode);
		free_index_branches(inode, &nr, &nr+1, (chain+n-1) - partial);
	}
	/* Clear the ends of indirect blocks on the shared branch */
	while (partial > chain) {
		free_index_branches(inode, partial->p + 1, index_block_end(partial->bh),
				(chain+n-1) - partial);
		mark_buffer_dirty_inode(partial->bh, inode);
		brelse (partial->bh);
		partial--;
	}
do_indirects:
	/* Kill the remaining (whole) subtrees */
	while (first_whole < IDXDEPTH-1) {
		nr = i_index[IDXDIRECT+first_whole];
		if (nr) {
			i_index[IDXDIRECT+first_whole] = 0;
			mark_inode_dirty(inode);
			free_index_branches(inode, &nr, &nr+1, first_whole+1);
		}
		first_whole++;
	}
	inode->i_mtime = inode->i_ctime = current_time(inode);
	mark_inode_dirty(inode);
}

/**
 * 根据文件内容中的偏移量offset，查找i_index[]管理的索引数据中索引该位置对应的数据记录的索引记录在索引数据中的偏移量。主要是用于truncate中，结果作为truncate_index()的一个输入参数。
 */
static indexblock_t getIndexPosWithDataOffset(struct inode * inode, indexblock_t offset){
	
	return -1;
}


static inline unsigned num_of_index_blocks(loff_t size, struct super_block *sb)
{
	int k = sb->s_blocksize_bits - 10;
	//这里减10是因为/usr/include/linux/fs.h中定义的两个宏#define BLOCK_SIZE_BITS 10和#define BLOCK_SIZE (1<<BLOCK_SIZE_BITS)，和我们定义的#define EPISODE_BLOCK_SIZE_BITS 12不同
	unsigned blocks, res, direct = IDXDIRECT, i = IDXDEPTH;
	blocks = (size + sb->s_blocksize - 1) >> (BLOCK_SIZE_BITS + k);
	res = blocks;
	while (--i && blocks > direct) {
		blocks -= direct;
		blocks += sb->s_blocksize/sizeof(indexblock_t) - 1;
		blocks /= sb->s_blocksize/sizeof(indexblock_t);
		res += blocks;
		direct = 1;
	}
	return res;
}


 int __episode_get_index_block(struct inode * inode, sector_t block,
			struct buffer_head *bh_result)
{
	//printk("in the __episode_get_index_block, bh_result=%p,&bh_result=%p",bh_result,&bh_result);
	return get_index_block_for_write(inode, block, bh_result);
}

unsigned episode_index_blocks(loff_t size, struct super_block *sb)
{
	return num_of_index_blocks(size, sb);
}
//一次写入一条索引数据
static int writeIndex(struct inode * inode, const struct timeIndex  * ti){
	struct episode_inode_info *episode_inode = episode_i(inode);
	indexblock_t indexnum = episode_inode->i_indexnum;
	sector_t blocknr = indexnum*TIMEINDEXLEN/EPISODE_BLOCK_SIZE;//逻辑块号
	__u32 innerPos = indexnum*TIMEINDEXLEN%EPISODE_BLOCK_SIZE;//
	struct buffer_head * bh;// = (struct buffer_head *) kmalloc(sizeof(struct buffer_head),GFP_KERNEL);
	//struct timeIndex *tia;//将bh->b_data视作TimeIndex的数组，tia指向头部
	int  err = -EIO;
	if((bh)==NULL) printk("the bh=NULL!");
	printk("[writeIndex()]before get index block, bh =%p, &bh=%p!",bh,&bh);
	//后期增加边界判断
	bh = get_index_block_bh_for_write(inode,blocknr,err);
	//err = __episode_get_index_block(inode, blocknr, bh);//bh在函数内进行处理，但是是局部变量，回来就变成了NULL
	printk("after get index block, &bh=%p,bh=%p",&bh,bh);
	if(bh){
		printk("buffer_head get successfully!");
	}else{
		printk("[indextree.h writeIndex()] bh=NULL!");
	}
//	tia = (struct timeIndex *)(bh->b_data);
	printk("[indextree.h writeIndex()] err:%d",err);
	printk("bh->b_blocknr = %d, size = %d",(bh)->b_blocknr,(bh)->b_size);
	printk("timeindexLen=%d,innerPos=%d,ti=%p",TIMEINDEXLEN,innerPos,ti);
	memcpy(bh->b_data+innerPos, ti,TIMEINDEXLEN);
	printk("ti->len:%d,ti->timestamp:%d, ti->offset:%d",ti->recLen,ti->timestamp,ti->offset);
	episode_inode->i_indexnum++;
	mark_buffer_dirty(bh);
	mark_inode_dirty(inode);
	return 1;

}
#endif
