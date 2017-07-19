BINARY=Release/ksequencer

# sudo setcap cap_sys_nice+eip $BINARY

valgrind --leak-check=yes $BINARY --port 16:0 &> valgrind.out
