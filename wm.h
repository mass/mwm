#include <stdio.h>
#include <unistd.h>
#include <X11/Xlib.h>

int main(int argc, char** argv);

int handle_configure_request(Display* display, XEvent* event);
int handle_map_request(Display* display, XEvent* event);

int handle_xerror(Display* display, XErrorEvent* e);

void wm_log(const char* msg);
