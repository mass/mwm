DISPLAY=:100.1 xclock &
DISPLAY=:100.1 xterm &
DISPLAY=:100.0 xterm &
exec ./build/mwm --display :100 --screen 0 --screen 1
