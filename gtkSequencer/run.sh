BINARY=Release/gtkSequencer

# sudo setcap cap_sys_nice+eip $BINARY
ulimit -c unlimited
rm core
$BINARY --port 16:0
