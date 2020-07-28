/**
本分析用于”数据和索引分段存储而单独管理“方案的开发过程参考。
*/

背景：前面的写入过程都是基于O_DIRECT模式进行的，所以写入的过程不涉及Page cache，本次要将数据和索引分段存储（段以block为单位），而blockid 分别管理的机制。
这样的好处是都可以使用page cache，而不用再为索引设计一套管理机制。这个方案需要解决的重要问题就是blockid的管理问题，也就是blockid是怎么放到i_data[10]进行管理的，
、blockid分配的时机，block和page cache建立映射的时机，映射机制、以及block是在什么时候申请的。 本文着重解决在buffered write过程中，这些核心问题是如何实现的。

1.调用过程
 //（1)用户态unistd.h中定义的ssize_t write(int fd, const void * buf, size_t count)
//    -->(2)include/linux/syscalls.h中定义的内核的sys_write()
//      -->(3)具体sys_write（）的实现定义在fs/read_write.c,具体为SYSCALL_DEFINE3(write, unsigned int, fd, const char __user *buf ,  size_t, count) 
//        -->(4)fs/read_write.c中的vfs_write(file, buf , count, &pos);
//          -->(4.1) 调用file_start_write(file)函数
//          -->(5) 调用同文件的__vfs_write(file,buf,count, pos)函数
//          -->file_start_write(file);//(4.2)
//          -->inc_syscw(current);//(4.3)
//          -->file_end_write(file);//(4.4)
//           -->(6)new_sync_write，这里假定file.c中设定的fie->f_op->write_iter绑定的函数应为generic_file_write_iter(在mm/filemap.c中),（目前采用O_DIRECT模式，绑定了episode_direct_write(),下面我们按照该函数往下分析。
//             -->init_sync_kiocb(&kiocb, filp);//(6.1)初始化kiocb，绑定kiocb和filp
//             -->iov_iter_init(&iter, WRITE, &iov, 1, len);//（6.2）初始化iov_iter，将iov绑定到iter中;
//             -->ret = call_write_iter(filp, &kiocb, &iter);//(7)调用真正的写入函数，在linux/fs.h中
//               -->file->f_op->write_iter(kio, iter);//（8）
//							   -->generic_file_write_iter(kio,iter);//(9)此为绑定的通用buffered write方法
//                   -->	ret = generic_write_checks(iocb, from);//（9.1）可写校验
//									 -->ret = __generic_file_write_iter(iocb, from);//（10）是（9）的具体实现
//									 --> generic_write_sync(iocb, ret);//(9.2)
//                      -->written = generic_perform_write(file, from, iocb->ki_pos);//（11）
//											  -->iov_iter_fault_in_readable(i, bytes);//(11.1)
//												-->fatal_signal_pending(current);//(11.2)
//												-->status = a_ops->write_begin(file, mapping, pos, bytes, flags, &page, &fsdata);//（11.3）
//													-->ret = block_write_begin(mapping, pos, len, flags, pagep, episode_get_block);//（11.3.1）fs/buffer.c中
//															-->page = grab_cache_page_write_begin(mapping, index, flags);//(11.3.1.1)从page cache中申请1个page，在include/linux/pagemap.h中
//																-->return find_or_create_page(mapping, index, mapping_gfp_mask(mapping)); //（11.3.1.1.1）
//																	-->	return pagecache_get_page(mapping, offset, FGP_LOCK|FGP_ACCESSED|FGP_CREAT,	gfp_mask);//(11.3.1.1.1.1）在mm/filemap.c中
//															-->status = __block_write_begin(page, pos, len, get_block);//(11.3.1.2)
//													-->episode_write_failed(mapping, pos + len);//（11.3.2）写入失败情况下的异常处理
//												-->if (mapping_writably_mapped(mapping))//（11.4）
//												-->   flush_dcache_page(page);//（11.5）
//												-->copied = iov_iter_copy_from_user_atomic(page, i, offset, bytes);//（12）将iov_iter中的内容复制到page中,在lib/iov_iter.c中
//												-->status = a_ops->write_end(file, mapping, pos, bytes, copied, page, fsdata);//（11.6）
//												-->cond_resched();//（11.7）
//												-->iov_iter_advance(i, copied);//（11.8）控制iov_iter，对数据不做操作，只是游标往前移动copied字节
//												-->balance_dirty_pages_ratelimited(mapping); //(11.9)看脏页是否太多，需要写回硬盘
//													-->iterate_all_kinds(i, bytes, v,	copyin((p += v.iov_len) - v.iov_len, v.iov_base, v.iov_len),
//																										memcpy_from_page((p += v.bv_len) - v.bv_len, v.bv_page, v.bv_offset, v.bv_len),
//																										memcpy((p += v.iov_len) - v.iov_len, v.iov_base, v.iov_len)) //（13）
到这里发现，分析的主干函数并没有涉及到blockid以及page cache中的页和磁盘中的block的映射建立联系。说明前面分析的主干有点问题，主要是（12）不能当作主干，它完成了iov_iter中的iov内容到pagecache中的页的拷贝过程。
我们回过头来，看看主干函数（11），在（12）前后重要的函数，先看（11.3）和（11.6）这俩函数。



2. 函数极其实现
(4)
ssize_t vfs_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	ssize_t ret;

	if (!(file->f_mode & FMODE_WRITE))//模式检查
		return -EBADF;
	if (!(file->f_mode & FMODE_CAN_WRITE))
		return -EINVAL;
	if (unlikely(!access_ok(VERIFY_READ, buf, count)))
		return -EFAULT;

	ret = rw_verify_area(WRITE, file, pos, count);//检查
	if (!ret) {
		if (count > MAX_RW_COUNT)
			count =  MAX_RW_COUNT;
		file_start_write(file);//（4.1）
		ret = __vfs_write(file, buf, count, pos);//（5）
		if (ret > 0) {
			fsnotify_modify(file);//(4.2)
			add_wchar(current, ret);(4.3)
		}
		inc_syscw(current);//(4.3)
		file_end_write(file);//(4.4)
	}

	return ret;
}
EXPORT_SYMBOL_GPL(vfs_write);

(5)
ssize_t __vfs_write(struct file *file, const char __user *p, size_t count,
		    loff_t *pos)
{
	if (file->f_op->write)//如果设置了write，则调用file->f_op->write指定的函数，这里假设我们没有设定该项。
		return file->f_op->write(file, p, count, pos);
	else if (file->f_op->write_iter)
		return new_sync_write(file, p, count, pos);//(6)
	else
		return -EINVAL;
}

(6) 
static ssize_t new_sync_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
	struct iovec iov = { .iov_base = (void __user *)buf, .iov_len = len };
	struct kiocb kiocb;
	struct iov_iter iter;
	ssize_t ret;

	init_sync_kiocb(&kiocb, filp);//(6.1)初始化kiocb，绑定kiocb和filp
	kiocb.ki_pos = *ppos;//设置kiocb的ki_pos变量，表示写入的位置
	iov_iter_init(&iter, WRITE, &iov, 1, len);//（6.2）初始化iov_iter，将iov绑定到iter中;

	ret = call_write_iter(filp, &kiocb, &iter);//(7)
	BUG_ON(ret == -EIOCBQUEUED);
	if (ret > 0)
		*ppos = kiocb.ki_pos;
	return ret;
}

（7） 
static inline ssize_t call_write_iter(struct file *file, struct kiocb *kio,
				      struct iov_iter *iter)
{
	return file->f_op->write_iter(kio, iter);//（8）调用真正的write接口
}

（9）
/**
 * generic_file_write_iter - write data to a file
 * @iocb:	IO state structure
 * @from:	iov_iter with data to write
 *
 * This is a wrapper around __generic_file_write_iter() to be used by most
 * filesystems. It takes care of syncing the file in case of O_SYNC file
 * and acquires i_mutex as needed.
 */
ssize_t generic_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct file *file = iocb->ki_filp;
	struct inode *inode = file->f_mapping->host;
	ssize_t ret;

	inode_lock(inode);
	ret = generic_write_checks(iocb, from);//（9.1）可写校验
	if (ret > 0)
		ret = __generic_file_write_iter(iocb, from);//（10）
	inode_unlock(inode);

	if (ret > 0)
		ret = generic_write_sync(iocb, ret);//(9.2)
	return ret;
}
EXPORT_SYMBOL(generic_file_write_iter);

(10)
/**
 * __generic_file_write_iter - write data to a file
 * @iocb:	IO state structure (file, offset, etc.)
 * @from:	iov_iter with data to write
 *
 * This function does all the work needed for actually writing data to a
 * file. It does all basic checks, removes SUID from the file, updates
 * modification times and calls proper subroutines depending on whether we
 * do direct IO or a standard buffered write.
 *
 * It expects i_mutex to be grabbed unless we work on a block device or similar
 * object which does not need locking at all.
 *
 * This function does *not* take care of syncing data in case of O_SYNC write.
 * A caller has to handle it. This is mainly due to the fact that we want to
 * avoid syncing under i_mutex.
 */
ssize_t __generic_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct file *file = iocb->ki_filp;
	struct address_space * mapping = file->f_mapping;
	struct inode 	*inode = mapping->host;
	ssize_t		written = 0;
	ssize_t		err;
	ssize_t		status;

	/* We can write back this queue in page reclaim */
	current->backing_dev_info = inode_to_bdi(inode);
	err = file_remove_privs(file);//删除权限
	if (err)
		goto out;

	err = file_update_time(file);//更新时间
	if (err)
		goto out;

	if (iocb->ki_flags & IOCB_DIRECT) {//这次默认不用O_DIRECT模式
		loff_t pos, endbyte;

		written = generic_file_direct_write(iocb, from);
		/*
		 * If the write stopped short of completing, fall back to
		 * buffered writes.  Some filesystems do this for writes to
		 * holes, for example.  For DAX files, a buffered write will
		 * not succeed (even if it did, DAX does not handle dirty
		 * page-cache pages correctly).
		 */
		if (written < 0 || !iov_iter_count(from) || IS_DAX(inode))
			goto out;

		status = generic_perform_write(file, from, pos = iocb->ki_pos);
		/*
		 * If generic_perform_write() returned a synchronous error
		 * then we want to return the number of bytes which were
		 * direct-written, or the error code if that was zero.  Note
		 * that this differs from normal direct-io semantics, which
		 * will return -EFOO even if some bytes were written.
		 */
		if (unlikely(status < 0)) {
			err = status;
			goto out;
		}
		/*
		 * We need to ensure that the page cache pages are written to
		 * disk and invalidated to preserve the expected O_DIRECT
		 * semantics.
		 */
		endbyte = pos + status - 1;
		err = filemap_write_and_wait_range(mapping, pos, endbyte);
		if (err == 0) {
			iocb->ki_pos = endbyte + 1;
			written += status;
			invalidate_mapping_pages(mapping,
						 pos >> PAGE_SHIFT,
						 endbyte >> PAGE_SHIFT);
		} else {
			/*
			 * We don't know how much we wrote, so just return
			 * the number of bytes which were direct-written
			 */
		}
	} else {
		written = generic_perform_write(file, from, iocb->ki_pos);//（11）
		if (likely(written > 0))
			iocb->ki_pos += written;
	}
out:
	current->backing_dev_info = NULL;
	return written ? written : err;
}
EXPORT_SYMBOL(__generic_file_write_iter);

(11)
ssize_t generic_perform_write(struct file *file,
				struct iov_iter *i, loff_t pos)
{
	struct address_space *mapping = file->f_mapping;
	const struct address_space_operations *a_ops = mapping->a_ops;
	long status = 0;
	ssize_t written = 0;
	unsigned int flags = 0;

	do {
		struct page *page;//用于存储当前要写入的内容的一个临时page？
		unsigned long offset;	/* Offset into pagecache page *///page内的偏移量，表示写的开始位置
		unsigned long bytes;	/* Bytes to write to page *///写多少个字节到page
		size_t copied;		/* Bytes copied from user *///从用户空间（也就是iov_iter的ivo中）拷贝的实际字节数
		void *fsdata;

		offset = (pos & (PAGE_SIZE - 1));//防止pos>PAGE_SIZE
		bytes = min_t(unsigned long, PAGE_SIZE - offset,
						iov_iter_count(i));//计算实际要写入的字节数

again:
		/*
		 * Bring in the user page that we will copy from _first_.
		 * Otherwise there's a nasty deadlock on copying from the
		 * same page as we're writing to, without it being marked
		 * up-to-date.
		 *
		 * Not only is this an optimisation, but it is also required
		 * to check that the address is actually valid, when atomic
		 * usercopies are used, below.
		 */
		if (unlikely(iov_iter_fault_in_readable(i, bytes))) {//iov_iter如果不可读？ 权限检查，（11.1）
			status = -EFAULT;
			break;
		}

		if (fatal_signal_pending(current)) {//（11.2）
			status = -EINTR;
			break;
		}

		status = a_ops->write_begin(file, mapping, pos, bytes, flags,
						&page, &fsdata);//（11.3）写入之前的准备工作
		if (unlikely(status < 0))
			break;

		if (mapping_writably_mapped(mapping))//（11.4）
			flush_dcache_page(page);//（11.5）

		copied = iov_iter_copy_from_user_atomic(page, i, offset, bytes);//（12）将iov_iter中的内容复制到page中
		flush_dcache_page(page);//（11.5）

		status = a_ops->write_end(file, mapping, pos, bytes, copied,
						page, fsdata);//（11.6）完成写入操作
		if (unlikely(status < 0))
			break;
		copied = status;

		cond_resched();//（11.7）

		iov_iter_advance(i, copied);//（11.8）控制iov_iter，对数据不做操作，只是游标往前移动copied字节
		if (unlikely(copied == 0)) {
			/*
			 * If we were unable to copy any data at all, we must
			 * fall back to a single segment length write.
			 *
			 * If we didn't fallback here, we could livelock
			 * because not all segments in the iov can be copied at
			 * once without a pagefault.
			 */
			bytes = min_t(unsigned long, PAGE_SIZE - offset,
						iov_iter_single_seg_count(i));
			goto again;
		}
		pos += copied;
		written += copied;

		balance_dirty_pages_ratelimited(mapping); //(11.9)
	} while (iov_iter_count(i));

	return written ? written : status;
}
EXPORT_SYMBOL(generic_perform_write);

(12)
size_t iov_iter_copy_from_user_atomic(struct page *page,
		struct iov_iter *i, unsigned long offset, size_t bytes)
{
	char *kaddr = kmap_atomic(page), *p = kaddr + offset;//（13）在高端内存区为page指针申请一个page，然后让p指向该地址之后的offset字节处，这就是将来要写入的地方
	//只处理1个正常页或者1个复合页
	if (unlikely(!page_copy_sane(page, offset, bytes))) {//（12.1）//判断这次写入的写是否会涉及到超过1个正常页或者1个复合页，如果超出，则释放页面并返回
		kunmap_atomic(kaddr);//释放从高端内存申请的那个page
		return 0;
	}
	if (unlikely(i->type & ITER_PIPE)) {//类型判断，我们不是PIPE
		kunmap_atomic(kaddr);
		WARN_ON(1);
		return 0;
	}
	iterate_all_kinds(i, bytes, v,
		copyin((p += v.iov_len) - v.iov_len, v.iov_base, v.iov_len),
		memcpy_from_page((p += v.bv_len) - v.bv_len, v.bv_page,
				 v.bv_offset, v.bv_len),
		memcpy((p += v.iov_len) - v.iov_len, v.iov_base, v.iov_len)
	) //（13）
	kunmap_atomic(kaddr);//释放前面申请的高端内存页
	return bytes;
}
EXPORT_SYMBOL(iov_iter_copy_from_user_atomic);

//（12.1）用于判断写入涉及到的页面只有1个普通页或者1个复合页，如果只有1个，则返回true
static inline bool page_copy_sane(struct page *page, size_t offset, size_t n)
{
	struct page *head;
	size_t v = n + offset;//结束位置

	/*
	 * The general case needs to access the page order in order
	 * to compute the page size.
	 * However, we mostly deal with order-0 （2的oder次方，也就是2^order）pages（这句话是说我们经常处理的页面都是1个） and thus can
	 * avoid a possible cache line miss for requests that fit all
	 * page orders.
	 */
	if (n <= v && v <= PAGE_SIZE)
		return true;
//v大于PAGE_SIZE，说明要写入内容涉及到多个页
//复合页就是将多个连续的页当成一个大页，第一个页是页头，最后一个是页尾
	head = compound_head(page);//判断是不是复合页，如果是，则返回复合页的链表头
	v += (page - head) << PAGE_SHIFT;//v的值还要增加前面的页的总长度，才是从页头开始计算的总长度，要看它是否会超出页尾

	if (likely(n <= v && v <= (PAGE_SIZE << compound_order(head))))//compound_oder(head)根据页头获取这个复合页的阶数，目的就是获取这个复合页包括了多少个页，然后计算这个复合页的总长度
		return true;
	WARN_ON(1);
	return false;
}

（13） i: iov_iter; n:number of bytes to copy, v:iovec(应该是i中的那个iov成员); I:
#define iterate_all_kinds(i, n, v, I, B, K) {			\
	if (likely(n)) {					\
		size_t skip = i->iov_offset;			\
		if (unlikely(i->type & ITER_BVEC)) {		\
			struct bio_vec v;			\
			struct bvec_iter __bi;			\
			iterate_bvec(i, n, v, __bi, skip, (B))	\
		} else if (unlikely(i->type & ITER_KVEC)) {	\
			const struct kvec *kvec;		\
			struct kvec v;				\
			iterate_kvec(i, n, v, kvec, skip, (K))	\
		} else {					\
			const struct iovec *iov;		\
			struct iovec v;				\
			iterate_iovec(i, n, v, iov, skip, (I))	\
		}						\
	}							\
}

static const struct address_space_operations episode_aops = {
	.readpage = episode_readpage,
	.writepage = episode_writepage,
	.write_begin = episode_write_begin,//绑定
	.write_end = generic_write_end,//绑定
	.direct_IO = episode_direct_IO,
	.bmap = episode_bmap
};

（11.3）

static int episode_write_begin(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned flags,
			struct page **pagep, void **fsdata)
{
	int ret;

	ret = block_write_begin(mapping, pos, len, flags, pagep, episode_get_block);//（11.3.1）
	if (unlikely(ret))
		episode_write_failed(mapping, pos + len);//（11.3.2）写入失败的异常处理

	return ret;
}

（11.3.1）
/*
 * block_write_begin takes care of the basic task of block allocation and
 * bringing partial write blocks uptodate first.
 *
 * The filesystem needs to handle block truncation upon failure.
 */
int block_write_begin(struct address_space *mapping, loff_t pos, unsigned len,
		unsigned flags, struct page **pagep, get_block_t *get_block)
{
	pgoff_t index = pos >> PAGE_SHIFT;//计算pos这个位置应该位于哪个page，得到page的序号（此时page可能还不存在），后面会跟基树关联上
	struct page *page;//一个空的page指针，还没有分配空间
	int status;

	page = grab_cache_page_write_begin(mapping, index, flags);//(11.3.1.1)
	if (!page)
		return -ENOMEM;

	status = __block_write_begin(page, pos, len, get_block);//(11.3.1.2)这个应该会涉及到获取blockid，但是否会放到i_data[10]进行管理，暂时不清楚
	if (unlikely(status)) {
		unlock_page(page);
		put_page(page);
		page = NULL;
	}

	*pagep = page;
	return status;
}
EXPORT_SYMBOL(block_write_begin);

(11.3.1.1)
/*
 * Returns locked page at given index in given cache, creating it if needed.
 */
static inline struct page *grab_cache_page(struct address_space *mapping,
								pgoff_t index)
{
	return find_or_create_page(mapping, index, mapping_gfp_mask(mapping)); //（11.3.1.1.1）
}

（11.3.1.1.1）
/**
 * find_or_create_page - locate or add a pagecache page
 * @mapping: the page's address_space
 * @index: the page's index into the mapping
 * @gfp_mask: page allocation mode
 *
 * Looks up the page cache slot at @mapping & @offset.  If there is a
 * page cache page, it is returned locked and with an increased
 * refcount.
 *
 * If the page is not present, a new page is allocated using @gfp_mask
 * and added to the page cache and the VM's LRU list.  The page is
 * returned locked and with an increased refcount.
 *
 * On memory exhaustion, %NULL is returned.
 *
 * find_or_create_page() may sleep, even if @gfp_flags specifies an
 * atomic allocation!
 */
static inline struct page *find_or_create_page(struct address_space *mapping,
					pgoff_t offset, gfp_t gfp_mask)
{
	return pagecache_get_page(mapping, offset,
					FGP_LOCK|FGP_ACCESSED|FGP_CREAT,
					gfp_mask);//(11.3.1.1.1.1)
}

#define FGP_ACCESSED		0x00000001
#define FGP_LOCK		0x00000002
#define FGP_CREAT		0x00000004
#define FGP_WRITE		0x00000008
#define FGP_NOFS		0x00000010
#define FGP_NOWAIT		0x00000020

(11.3.1.1.1.1)

/**
 * pagecache_get_page - find and get a page reference
 * @mapping: the address_space to search
 * @offset: the page index
 * @fgp_flags: PCG flags
 * @gfp_mask: gfp mask to use for the page cache data page allocation
 *
 * Looks up the page cache slot at @mapping & @offset.
 *
 * PCG flags modify how the page is returned.
 *
 * @fgp_flags can be:
 *
 * - FGP_ACCESSED: the page will be marked accessed
 * - FGP_LOCK: Page is return locked
 * - FGP_CREAT: If page is not present then a new page is allocated using
 *   @gfp_mask and added to the page cache and the VM's LRU
 *   list. The page is returned locked and with an increased
 *   refcount. Otherwise, NULL is returned.
 *
 * If FGP_LOCK or FGP_CREAT are specified then the function may sleep even
 * if the GFP flags specified for FGP_CREAT are atomic.
 *
 * If there is a page cache page, it is returned with an increased refcount.
 */
struct page *pagecache_get_page(struct address_space *mapping, pgoff_t offset,
	int fgp_flags, gfp_t gfp_mask)
{
	struct page *page;

repeat:
	page = find_get_entry(mapping, offset);
	if (radix_tree_exceptional_entry(page))
		page = NULL;
	if (!page)
		goto no_page;

	if (fgp_flags & FGP_LOCK) {
		if (fgp_flags & FGP_NOWAIT) {
			if (!trylock_page(page)) {
				put_page(page);
				return NULL;
			}
		} else {
			lock_page(page);
		}

		/* Has the page been truncated? */
		if (unlikely(page->mapping != mapping)) {
			unlock_page(page);
			put_page(page);
			goto repeat;
		}
		VM_BUG_ON_PAGE(page->index != offset, page);
	}

	if (page && (fgp_flags & FGP_ACCESSED))
		mark_page_accessed(page);

no_page:
	if (!page && (fgp_flags & FGP_CREAT)) {
		int err;
		if ((fgp_flags & FGP_WRITE) && mapping_cap_account_dirty(mapping))
			gfp_mask |= __GFP_WRITE;
		if (fgp_flags & FGP_NOFS)
			gfp_mask &= ~__GFP_FS;

		page = __page_cache_alloc(gfp_mask);
		if (!page)
			return NULL;

		if (WARN_ON_ONCE(!(fgp_flags & FGP_LOCK)))
			fgp_flags |= FGP_LOCK;

		/* Init accessed so avoid atomic mark_page_accessed later */
		if (fgp_flags & FGP_ACCESSED)
			__SetPageReferenced(page);

		err = add_to_page_cache_lru(page, mapping, offset, gfp_mask);
		if (unlikely(err)) {
			put_page(page);
			page = NULL;
			if (err == -EEXIST)
				goto repeat;
		}
	}

	return page;
}
EXPORT_SYMBOL(pagecache_get_page);