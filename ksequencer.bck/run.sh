BINARY=Release/ksequencer

# sudo setcap cap_sys_nice+eip $BINARY
ulimit -c unlimited
rm -rf core
$BINARY --port 16:0
