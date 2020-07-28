obj-m += episode.o
episode-objs := bitmap.o namei.o inode.o file.o dir.o tool.o

ccflags-y += -D__KERNEL__

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

insmod: #模块加载
	sudo insmod episode.ko

mount: #文件mount
    #sudo dd if=/dev/zero  of=$(EPOSIDEFS_DISK)  bs=1k  count=10240
	sudo mount -t episodefs -o $(EPOSIDEFS_DISK) /mnt/episode_FS

umount: #文件unmont和模块卸载
	sudo umount /mnt/episode
	sudo rmmod episodefs.ko

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

