sudo rmmod noodle 2>/dev/null
make
sudo insmod noodle.ko
make clean
