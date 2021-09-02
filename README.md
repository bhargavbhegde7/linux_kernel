Learning linux kernel modules development

This repository is for learning and implementing linux kernel modules.
If you want to see it work, pleas cd into the fake_device directory and run the following:

1. make clean && make (this will build the module)
2. sudo insmod fake_module.ko
3. gcc producer.c -o producer
4. gcc consumer.c -o consumer
5. sudo mknod /dev/batmobile c 236 0 (the number 236 is to be taken from the dmesg system kernal logs)
6. sudo chmod 666 /dev/batmobile
7. ./producer (separate terminal)
8. ./consumer (separate terminal)

After these, you can enter messages in the producer terminal and it will appear on the consumer terminal.

for usb driver:
https://www.youtube.com/watch?v=5IDL070RtoQ&list=PL16941B715F5507C5&index=9

for mock driver implementation
https://www.youtube.com/watch?v=_4H-F_5yDvo&list=PL16941B715F5507C5&index=6

Other useful blog post:
1. http://derekmolloy.ie/writing-a-linux-kernel-module-part-1-introduction/
2. https://sysprog21.github.io/lkmpg/
