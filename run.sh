#!/bin/bash
#xinit ./xinitrc -- /usr/bin/Xephyr :100 -screen 1200x800 -screen 1200x800

DISPLAY=:1.1 xclock &
DISPLAY=:1.1 xterm &
DISPLAY=:1.1 terminator -m &
./build/mwm --display :1 --screen 1
