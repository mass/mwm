#include "wm.h"

int main(int argc, char** argv) {
  wm_log("Begin execution");

  Display* display = XOpenDisplay(NULL);
  if (display == NULL) {
    wm_log("No display!");
    return 1;
  }

  Window root = DefaultRootWindow(display);

  XSetErrorHandler(&handle_xerror);
  XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask);
  XSync(display, 0);

  // Main event loop
  for (;;) {
    XEvent e;
    XNextEvent(display, &e);

    switch (e.type) {
      case ConfigureRequest:
        handle_configure_request(display, &e);
        break;
      case MapRequest:
        handle_map_request(display, &e);
        break;
      default:
        wm_log("Not yet handled");
        break;
    }
  }

  XCloseDisplay(display);
  return 0;
}

int handle_configure_request(Display* display, XEvent* event) {
  wm_log("ConfigureRequest");
  XConfigureRequestEvent e = *((XConfigureRequestEvent*) event);

  XWindowChanges changes;
  changes.x = e.x;
  changes.y = e.y;
  changes.width = e.width;
  changes.height = e.height;

  XConfigureWindow(display, e.window, e.value_mask, &changes);
  wm_log("End ConfigureRequest");
}

int handle_map_request(Display* display, XEvent* event) {
  wm_log("MapRequest");
  XMapRequestEvent e = *((XMapRequestEvent*) event);

  XMapWindow(display, e.window);
  wm_log("End MapRequest");
}

int handle_xerror(Display* display, XErrorEvent* e) {
  wm_log("Error!");
  return 1;
}

void wm_log(const char* msg) {
  fprintf(stdout, "[wm]: %s\n", msg);
}

