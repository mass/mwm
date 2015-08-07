#!/bin/bash
set -e

make

xinit ./xinitrc -- /usr/bin/Xephyr :100 -screen 800x600
