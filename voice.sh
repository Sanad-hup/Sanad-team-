#!/bin/bash
if pgrep -f voice.py > /dev/null; then
    pkill -f voice.py
else
    setsid python3 /home/firstteam/sanad/voice.py >/dev/null 2>&1 &
fi
