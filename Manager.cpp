#include "Manager.hpp"

#include <mass/Log.hpp>

#include <X11/Xutil.h>

#include <algorithm>

// Static error handler for XLib
static int XError(Display* display, XErrorEvent* e) {
  char buf[1024];
  XGetErrorText(display, e->error_code, buf, sizeof(buf));

  LOG(ERROR) << "X Error"
             << " display=" << DisplayString(display)
             << " what=(" << buf << ")";
  return 0;
}

Manager::Manager(const std::string& display,
                 const std::vector<int>& screens)
  : _argDisp(display)
  , _argScreens(screens)
{
  _drag.w = 0;
}

Manager::~Manager()
{
  if (_disp != nullptr) XCloseDisplay(_disp);
}

bool Manager::init()
{
  XSetErrorHandler(&XError);

  _disp = XOpenDisplay(_argDisp.c_str());
  if (_disp == nullptr) {
    LOG(ERROR) << "failed to open X display=" << _argDisp;
    return false;
  }

  _numScreens = ScreenCount(_disp);
  LOG(INFO) << "display=" << DisplayString(_disp) << " screens=" << _numScreens;

  for (int i = 0; i < _numScreens; ++i) {
    if (std::count(begin(_argScreens), end(_argScreens), i) == 0) continue;

    auto root = RootWindow(_disp, i);
    LOG(INFO) << "screen=" << DisplayString(_disp) << "." << i << " root=" << root;
    _roots[root] = i;

    XSelectInput(_disp, root, SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_Tab), Mod1Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_D), Mod1Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_T), Mod1Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_M), Mod1Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabButton(_disp, 1, 0, root, false,
                ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(_disp, 1, Mod1Mask, root, false,
                ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(_disp, 3, Mod1Mask, root, false,
                ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);
  }

  return true;
}

void Manager::run()
{
  // Main event loop
  for (;;) {
    XEvent e;
    XNextEvent(_disp, &e);

    switch (e.type) {
      // Ignore these events
      case ReparentNotify:
      case MapNotify:
      case ConfigureNotify:
      case CreateNotify:
      case DestroyNotify:
      case KeyRelease:
      case ButtonRelease:
        break;

      case MapRequest:
        onReq_Map(e.xmaprequest);
        break;
      case ConfigureRequest:
        onReq_Configure(e.xconfigurerequest);
        break;

      case UnmapNotify:
        onNot_Unmap(e.xunmap);
        break;
      case MotionNotify:
        while (XCheckTypedWindowEvent(_disp, e.xmotion.window, MotionNotify, &e)); // Get latest
        onNot_Motion(e.xbutton);
        break;

      case KeyPress:
        onKeyPress(e.xkey);
        break;
      case ButtonPress:
        onBtnPress(e.xbutton);
        break;

      default:
        LOG(ERROR) << "XEvent not yet handled type=" << e.type
                   << " event=(" << ToString(e) << ")";
        break;
    }
  }
}

void Manager::onReq_Map(const XMapRequestEvent& e)
{
  LOG(INFO) << "request=Map window=" << e.window;
  addClient(e.window);
}

void Manager::onReq_Configure(const XConfigureRequestEvent& e)
{
  LOG(INFO) << "request=Configure window=" << e.window;

  XWindowChanges changes;
  changes.x = e.x;
  changes.y = e.y;
  changes.width = e.width;
  changes.height = e.height;
  changes.border_width = e.border_width;

  XConfigureWindow(_disp, e.window, e.value_mask, &changes);
}

void Manager::onNot_Unmap(const XUnmapEvent& e)
{
  LOG(INFO) << "notify=Unmap window=" << e.window;

  auto it = _clients.find(e.window);
  if (it != end(_clients)) {
    delClient(it->second);
  }
}

void Manager::onNot_Motion(const XButtonEvent& e)
{
  if (_drag.w == 0) return;

  auto client = _drag.w;
  if (_clients.find(client) == end(_clients)) {
    LOG(ERROR) << "client not found for motion event client=" << client;
    return;
  }

  int xdiff = e.x_root - _drag.xR;
  int ydiff = e.y_root - _drag.yR;

  if (_drag.btn == 1) {
    XMoveWindow(_disp, client, _drag.x + xdiff, _drag.y + ydiff);
  } else if (_drag.btn == 3) {
    XResizeWindow(_disp, client,
                  std::max(1, _drag.width + xdiff), std::max(1, _drag.height + ydiff));
  }
}

void Manager::onKeyPress(const XKeyEvent& e)
{
  //LOG(INFO) << "keyPress window=" << e.window << " subwindow=" << e.subwindow
  //          << " keyCode=" << e.keycode;

  if ((e.state & Mod1Mask) &&
      (e.keycode == XKeysymToKeycode(_disp, XK_Tab)))
  {
    LOG(INFO) << "got ALT-TAB window=" << e.window << " subwindow=" << e.subwindow;
    if (e.subwindow != 0) XRaiseWindow(_disp, e.subwindow);
  }

  if ((e.state & Mod1Mask) &&
      (e.keycode == XKeysymToKeycode(_disp, XK_T)))
  {
    LOG(INFO) << "got WIN-T window=" << e.window;

    auto it = _roots.find(e.window);
    if (it == end(_roots)) {
      LOG(ERROR) << "can't find root=" << e.window;
      return;
    }
    int screen = it->second;
    LOG(INFO) << "root=" << e.window << " screen=" << screen;

    std::ostringstream cmd;
    cmd << "DISPLAY=" << DisplayString(_disp) << "." << screen;
    cmd << " terminator -m &";
    LOG(INFO) << "starting cmd=(" << cmd.str() << ")";
    system(cmd.str().c_str());
  }

  if ((e.state & Mod1Mask) &&
      (e.keycode == XKeysymToKeycode(_disp, XK_M)))
  {
    auto it = _clients.find(e.subwindow);
    if (it == end(_clients)) {
      LOG(ERROR) << "can't find client=" << e.subwindow;
      return;
    }
    auto& client = it->second;

    XWindowAttributes attr;
    XGetWindowAttributes(_disp, client.root, &attr);

    XWindowChanges changes;
    changes.x = 0;
    changes.y = 0;
    changes.width = attr.width;
    changes.height = attr.height;

    XConfigureWindow(_disp, client.client, CWX | CWY | CWWidth | CWHeight, &changes);
  }

  if ((e.state & Mod1Mask) &&
      (e.keycode == XKeysymToKeycode(_disp, XK_D)))
  {
    if (e.subwindow == 0) return;

    LOG(INFO) << "got WIN-D window=" << e.window << " subwindow=" << e.subwindow;

    XEvent event;
    event.xclient.type = ClientMessage;
    event.xclient.window = e.subwindow;
    event.xclient.message_type = XInternAtom(_disp, "WM_PROTOCOLS", true);
    event.xclient.format = 32;
    event.xclient.data.l[0] = XInternAtom(_disp, "WM_DELETE_WINDOW", false);
    event.xclient.data.l[1] = CurrentTime;
    XSendEvent(_disp, e.subwindow, false, NoEventMask, &event);
  }
}

void Manager::onBtnPress(const XButtonEvent& e)
{
  if (e.subwindow == 0) return;

  LOG(INFO) << "btnPress window=" << e.window << " subwindow=" << e.subwindow << " " << e.state;

  // Normal click
  if (e.state == 0) {
    XRaiseWindow(_disp, e.subwindow);
    XSetInputFocus(_disp, e.subwindow, RevertToPointerRoot, CurrentTime);
    _drag.w = 0;
  }

  // Alt-click
  if (e.state & Mod1Mask) {
    _drag.btn = e.button;
    _drag.xR = e.x_root;
    _drag.yR = e.y_root;
    _drag.w = e.subwindow;

    XWindowAttributes attr;
    XGetWindowAttributes(_disp, e.subwindow, &attr);
    _drag.x = attr.x;
    _drag.y = attr.y;
    _drag.width = attr.width;
    _drag.height = attr.height;

    XRaiseWindow(_disp, e.subwindow);
    XSetInputFocus(_disp, e.subwindow, RevertToPointerRoot, CurrentTime);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// TODO
void Manager::addClient(Window w)
{
  auto it = _clients.find(w);
  if (it != end(_clients)) {
    LOG(ERROR) << "window=" << w << " is already framed!";
    return;
  }

  Client c;
  c.client = w;
  c.root = getRoot(w);
  _clients.insert({w, c});

  XMapWindow(_disp, w);

  LOG(INFO) << "added client=" << w;
}

void Manager::delClient(Client& c)
{
  _clients.erase(c.client);
  LOG(INFO) << "deleted client=" << c.client;
}

Window Manager::getRoot(Window w)
{
  Window root, parent;
  Window* children;
  uint32_t num;

  XQueryTree(_disp, w, &root, &parent, &children, &num);

  return root;
}
