# independentIndex
独立索引的设计与实现


# v0.1 实现了单调索引数据的写入
 存在的问题：
 （1）get_index_block_bh_for_write()函数可能存在bh返回时，bh被释放的问题。这个可能需要采用结构体指针的引用来解决。
 （2）writeIndex()需要增加处理多条索引数据的能力
 （3）目前的写入采用的是Direct 模式，后面可能采用buffered IO模式，这一部分可能需要跟着变。
 （4）写入多条的时候，i_index[]和i_indexnum正常变化，但索引内容只存入了一条，这个应该是writeIndex()中进行memcpy()时指针位置没有往前移动，造成的。主要是mempcy里面的参数不正确：bh->b_data+innerPos=000000007e055363,bh->b_data=0000000095c18186，此时innerPos=144,显然存在矛盾

 # v0.2 完成了多次打开关闭写入，以及多次连续索引写入的功能
目前，可以实现一次顺序多次写入索引数据，也可以多次打开、关闭、写入操作。

