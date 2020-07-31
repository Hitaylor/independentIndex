#include "episode.h"
#include <uapi/linux/uio.h>
#include <linux/uio.h>
#include "indextree.h"
#include <linux/slab.h>
//#include <>
//#include "timeIndex.h"
static int episode_setattr(struct dentry *dentry, struct iattr *attr)
{
    struct inode *inode = d_inode(dentry);
    int error;

    error = setattr_prepare(dentry, attr);
    if (error)
      return error;

    if ((attr->ia_valid & ATTR_SIZE) && attr->ia_size != i_size_read(inode)) {
      error = inode_newsize_ok(inode, attr->ia_size);
      if (error)
        return error;

      truncate_setsize(inode, attr->ia_size);
      episode_truncate(inode);
    }

    setattr_copy(inode, attr);
    mark_inode_dirty(inode);
    return 0;
}

static inline int set_iocb_flags(struct file *file)
{
        int res = 0;
	res |= IOCB_APPEND;
	res |= IOCB_DIRECT;
	res |= IOCB_SYNC;
        return res;
}

static inline enum rw_hint write_hint(struct file *file)
{
        if (file->f_write_hint != WRITE_LIFE_NOT_SET)
                return file->f_write_hint;

        return file_inode(file)->i_write_hint;
}

static inline void init_kiocb(struct kiocb *kiocb, struct file *filp)
{
        *kiocb = (struct kiocb) {
                .ki_filp = filp,
                .ki_flags = set_iocb_flags(filp),
                .ki_hint = write_hint(filp),
        };
}

static ssize_t episode_direct_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
	struct iovec iov = { .iov_base = (void __user *)buf, .iov_len = len };
	struct kiocb kiocb;
	struct iov_iter iter;
	ssize_t ret;
        struct inode * inode;
        struct timeIndex * ti;

        inode = file_inode(filp);
	init_kiocb(&kiocb, filp);
	kiocb.ki_pos = *ppos;
	iov_iter_init(&iter, WRITE, &iov, 1, len);

	ret = generic_file_write_iter(&kiocb, &iter);
        //ret>0时表示写入的数据量（字节为单位）
        BUG_ON(ret == -EIOCBQUEUED);
         
        

        //写入索引数据的时机是什么？ret>0还是ret=-EIOCBQUEUED也行？这里将来要加固
        if(ret >0){
        ti = (struct timeIndex *)kmalloc(sizeof(struct timeIndex),GFP_KERNEL);//需要安全的申请空间       
                printk("[episode_direct_write()]ret=%d ppos=%d",ret,*ppos);
                ti->offset = inode->i_size;
                ti->recLen = len;
                ti->timestamp = getCurrentTime();
                printk("[episode_direct_write()] len=%d, timestamp=%u",len,ti->timestamp);
                writeIndex(inode,ti);
        kfree(ti);//释放指针                 
        }   
        //先写索引，再更新i_size;
        if (ret > 0)
        {
                *ppos = kiocb.ki_pos;

        }  
        
        return ret;
}

/*
 * We have mostly NULLs here: the current defaults are OK for
 * the episode filesystem.
 */
const struct file_operations episode_file_operations = {
    .llseek	= generic_file_llseek,
    .read_iter	= generic_file_read_iter,
    //.write_iter      = generic_file_write_iter,
    .write	= episode_direct_write,
    .mmap	= generic_file_mmap,
    .fsync		= generic_file_fsync,
    .splice_read	= generic_file_splice_read,
};

const struct inode_operations episode_file_inode_operations = {
	.setattr	= episode_setattr,
	.getattr	= episode_getattr,
};
