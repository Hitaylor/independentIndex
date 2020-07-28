# independentIndex
独立索引的设计与实现


# v0.1 实现了单调索引数据的写入
 存在的问题：
 （1）get_index_block_bh_for_write()函数可能存在bh返回时，bh被释放的问题。这个可能需要采用结构体指针的引用来解决。
 （2）writeIndex()需要增加处理多条索引数据的能力
 （3）目前的写入采用的是Direct 模式，后面可能采用buffered IO模式，这一部分可能需要跟着变。