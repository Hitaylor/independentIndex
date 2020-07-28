#!/bin/bash
echo "Delete the /mnt/episode/t1.txt"
rm /mnt/episode/t1.txt
echo "umount the episode FS"
umount -d /mnt/episode/
echo "remove the episodefs module from the kernel!"
rmmod episode
echo "clean the old episode module related file (*.ko)!"
cd /home/ok/code/episodeFS/episodeFS/episodefs/
make clean
echo "clear finished! Now, rebuild the episode fs module!"
make
echo "insmod the episodefs module into kernel!"
insmod episode.ko
echo "insmod finished!"

echo "losetup -f"
losetup -f
echo "setup the file 07271012 as the loop0 device!"
cd /home/ok/code/episodeFS/episodeFS/data/
losetup -f 07271012
echo "mount the /dev/loop0 to /mnt/episode/"
mount /dev/loop0 /mnt/episode/
#echo "mount fs finished!"
echo "delete t1.txt"
rm /mnt/episode/t1.txt
echo "write test!"
cd /home/ok/code/episodeFS/episodeFS/test/
gcc dioTest.c -o dioTest
./dioTest
echo "write with index finished!"
#gcc readTest.c -o readTest
#./readTest
