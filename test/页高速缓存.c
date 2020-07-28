[高速缓存的写操作]
对高速缓存的写操作，主要来自于其它函数对高速缓存的使用。写操作的使用方式很多，例如在inode.c的write_node函数。
write_node函数在调用是需要传递一个指向inode节点的指针，并且设置好这个i节点的对应的设备号和节点号。write_node
函数的作用是将一个i节点的信息写入设备中(其实是写入高速缓存中)。这里摘抄部分与高速缓存相关的部分
                    if (!(sb=get_super(inode->i_dev)))//获取sb，也可以使用inode的指针获取sb
                                panic( "trying to write inode without device" );
              
                block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks +
                                (inode->i_num-1)/INODES_PER_BLOCK;//计算文件系统中的blockid
                 
                 if (!(bh=bread(inode->i_dev,block)))//读取指定的block到高速缓存，并放回指向该缓存的bh
                                panic( "unable to read i-node block" );
      /*前面部分是得到了i节点所在逻辑块的高速缓存，bh指向这个高速缓存头*/
                (( struct d_inode *)bh->b_data)[(inode->i_num-1)%INODES_PER_BLOCK] =
                                                *( struct d_inode *)inode;//高速缓存当成1个数组，里面存储的就是inode（因为定长），这里也可以把高速缓存当成索引（定长）数组
              
                bh->b_dirt=1;//如果内容更改，比如写入索引数据，则设置脏位
                inode->i_dirt=0;
                brelse(bh);//高速缓存用完，就可以释放了
对高速缓存进行写操作的部分为(( struct d_inode *)bh->b_data)[(inode->i_num-1)%INODES_PER_BLOCK] = *( struct d_inode *)inode;
由于之前读入高速缓存的数据是i节点所在的逻辑块的整块数据，而这个逻辑块是由许多的设备i节点组成的。因此可以将这个逻辑块的数据看成设备上i节点的数组。
所以(struct d_inode*)bd->b_data，将缓存数据块当作设备i节点数组来了解。inode->i_num%INODES_PER_BLOCK，根据i节点号计算这个i节点在设备i节
点数组上的索引。最后(( struct d_inode *)bh->b_data)[(inode->i_num-1)%INODES_PER_BLOCK] = *( struct d_inode *)inode;将i节点数据
写入了缓存中。在对缓存写操作结束之后，bh->b_dirt=1设置缓存信息，表明缓存中数据被修改，其它进程如果需要使用这个缓存，就需要先把数据同步到设备中。
brelse(bh)在缓存使用结束后释放缓存，在后面将会介绍这个函数的代码实现。
————————————————
版权声明：本文为CSDN博主「王炎林」的原创文章，遵循CC 4.0 BY-SA版权协议，转载请附上原文出处链接及本声明。
原文链接：https://blog.csdn.net/yanlinwang/article/details/8307819