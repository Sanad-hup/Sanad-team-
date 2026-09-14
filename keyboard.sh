#!/bin/bash
if pgrep -x squeekboard > /dev/null; then
    pkill -x squeekboard
else
    setsid squeekboard >/dev/null 2>&1 &
fi
