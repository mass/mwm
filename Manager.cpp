#include "Manager.hpp"

#include <mass/Log.hpp>

#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrandr.h>

#include <algorithm>
#include <math.h>
#include <optional>

////////////////////////////////////////////////////////////////////////////////
/// Keyboard / Mouse Shortcuts
///
/// Super         + h,j,k,l | Move focus to other window
/// Super + Shift + h,j,k,l | TODO
/// Super + Alt   + h,j,k,l | Move window in grid on current monitor
/// Super + Ctrl  + h,j,k,l | Move window to other monitor
///
/// Super + D | Close the window that currently has focus
/// Super + T | Open a terminal
/// Super + M | Maximize the current window
/// Super + N | Restore/unmaximize the current window

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
  if (_disp != nullptr)
    XCloseDisplay(_disp);
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
    ScreenInfo s;
    s.num = i;

    XSelectInput(_disp, root, SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask | ButtonPressMask | FocusChangeMask);

    // Grab keys with Super/GUI modifier with and without NUMLOCK modifier
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_Tab), Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_D),   Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_T),   Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_M),   Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_N),   Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_H),   Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_J),   Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_K),   Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_L),   Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_Tab), Mod2Mask | Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_D),   Mod2Mask | Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_T),   Mod2Mask | Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_M),   Mod2Mask | Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_N),   Mod2Mask | Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_H),   Mod2Mask | Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_J),   Mod2Mask | Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_K),   Mod2Mask | Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, XK_L),   Mod2Mask | Mod4Mask, root, false, GrabModeAsync, GrabModeAsync);

    // Set the background
    XSetWindowBackground(_disp, root, 0x604020);
    XClearWindow(_disp, root);

    // Less ugly cursor
    Cursor cursor = XCreateFontCursor(_disp, XC_crosshair);
    XDefineCursor(_disp, root, cursor);

    // Identify monitors on this X screen
    XRRScreenResources* res = XRRGetScreenResourcesCurrent(_disp, root);
    for (int j = 0; j < res->noutput; ++j) {
      XRROutputInfo* monitor = XRRGetOutputInfo(_disp, res, res->outputs[j]);
      if (monitor->connection) continue; // No monitor plugged in
      XRRCrtcInfo* crtc = XRRGetCrtcInfo(_disp, res, monitor->crtc);
      LOG(INFO) << "found monitor for"
                << " screen=" << i
                << " name=" << monitor->name
                << " width=" << crtc->width
                << " height=" << crtc->height
                << " xPos=" << crtc->x
                << " yPos=" << crtc->y;
      Monitor m;
      m.r.o.x = crtc->x;
      m.r.o.y = crtc->y;
      m.r.w = crtc->width;
      m.r.h = crtc->height;
      m.name = monitor->name;
      s.monitors.push_back(m);
    }

    _screens.insert({root, s});

    // Add pre-existing windows on this screen
    XGrabServer(_disp);
    Window root2, parent; Window* children; uint32_t num;
    XQueryTree(_disp, root, &root2, &parent, &children, &num);
    for (uint32_t j = 0; j < num; ++j) {
      addClient(children[j], true);
    }
    XFree(children);
    XUngrabServer(_disp);
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
      case MappingNotify:
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

      case FocusIn:
        handleFocusChange(e.xfocus.window, true);
        break;
      case FocusOut:
        handleFocusChange(e.xfocus.window, false);
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
  addClient(e.window, false);
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

  XConfigureWindow(_disp, e.window, (unsigned) e.value_mask, &changes);
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
    // Alt-LeftClick moves window around
    XMoveWindow(_disp, client, _drag.x + xdiff, _drag.y + ydiff);
  }
  else if (_drag.btn == 3) {
    // Alt-RightClick resizes
    int nx, ny, nw, nh;
    switch(_drag.dirVert) {
      case DIR::Up:
        ny = _drag.y + ydiff;
        nh = std::max(25, _drag.height - ydiff);
        break;
      case DIR::Down:
        ny = _drag.y;
        nh = std::max(25, _drag.height + ydiff);
        break;
      default:
        ny = _drag.y;
        nh = _drag.height;
        break;
    }
    switch(_drag.dirHorz) {
      case DIR::Left:
        nx = _drag.x + xdiff;
        nw = std::max(25, _drag.width - xdiff);
        break;
      case DIR::Right:
        nx = _drag.x;
        nw = std::max(25, _drag.width + xdiff);
        break;
      default:
        nx = _drag.x;
        nw = _drag.width;
        break;
    }
    XMoveResizeWindow(_disp, client, nx, ny, nw, nh);
  }
}

void Manager::onKeyPress(const XKeyEvent& e)
{
  LOG(INFO) << "keyPress window=" << e.window << " subwindow=" << e.subwindow
            << " keyCode=" << e.keycode << " state=" << e.state;

  if ((e.state & Mod4Mask) &&
      (e.keycode == XKeysymToKeycode(_disp, XK_Tab)))
  {
    LOG(INFO) << "got ALT-TAB window=" << e.window << " subwindow=" << e.subwindow;
    if (e.subwindow != 0)
      XRaiseWindow(_disp, e.subwindow);
    return;
  }

  if ((e.state & Mod4Mask) &&
      (e.keycode == XKeysymToKeycode(_disp, XK_T)))
  {
    LOG(INFO) << "got WIN-T window=" << e.window;

    auto it = _screens.find(e.window);
    if (it == end(_screens)) {
      LOG(ERROR) << "can't find root=" << e.window;
      return;
    }
    int screen = it->second.num;
    LOG(INFO) << "root=" << e.window << " screen=" << screen;

    std::ostringstream cmd;
    cmd << "DISPLAY=" << DisplayString(_disp) << "." << screen;
    cmd << " terminator -m &";
    LOG(INFO) << "starting cmd=(" << cmd.str() << ")";
    system(cmd.str().c_str());
    return;
  }

  if ((e.state & Mod4Mask) &&
      (e.keycode == XKeysymToKeycode(_disp, XK_H) ||
       e.keycode == XKeysymToKeycode(_disp, XK_J) ||
       e.keycode == XKeysymToKeycode(_disp, XK_K) ||
       e.keycode == XKeysymToKeycode(_disp, XK_L)))
  {
    DIR dir;
    if (e.keycode == XKeysymToKeycode(_disp, XK_H)) {
      dir = DIR::Left;
    } else if(e.keycode == XKeysymToKeycode(_disp, XK_J)) {
      dir = DIR::Down;
    } else if(e.keycode == XKeysymToKeycode(_disp, XK_K)) {
      dir = DIR::Up;
    } else if(e.keycode == XKeysymToKeycode(_disp, XK_L)) {
      dir = DIR::Right;
    }

    Window curFocus; int curRevert;
    XGetInputFocus(_disp, &curFocus, &curRevert);
    if (_clients.find(curFocus) == end(_clients)) {
      curFocus = getRoot(curFocus);
    }

    Window nextFocus = getNextWindowInDir(dir, curFocus);
    switchFocus(nextFocus);
    return;
  }

  if ((e.state & Mod4Mask) &&
      (e.keycode == XKeysymToKeycode(_disp, XK_M)))
  {
    Window curFocus; int curRevert;
    XGetInputFocus(_disp, &curFocus, &curRevert);

    auto it = _clients.find(curFocus);
    if (it == end(_clients)) {
      LOG(ERROR) << "can't find client=" << e.subwindow;
      return;
    }
    auto& client = it->second;
    if (client.ign) return;

    XWindowAttributes attr;
    XGetWindowAttributes(_disp, client.client, &attr);
    Point c = getCenter(attr.x, attr.y, attr.width, attr.height);

    const auto& monitors = _screens[client.root].monitors;
    auto it2 = std::find_if(begin(monitors), end(monitors),
                            [c] (const auto& m) { return m.r.contains(c); });
    if (it2 == end(monitors)) {
      LOG(ERROR) << "no monitor contains (" << c.x << "," << c.y << ")";
      return;
    }
    auto& mon = *it2;

    if (attr.width == mon.r.w && attr.height == mon.r.h) return;

    client.preMax.o.x = attr.x;
    client.preMax.o.y = attr.y;
    client.preMax.w = attr.width;
    client.preMax.h = attr.height;

    XWindowChanges changes;
    changes.x = mon.r.o.x;
    changes.y = mon.r.o.y;
    changes.width = mon.r.w;
    changes.height = mon.r.h;
    XConfigureWindow(_disp, client.client, CWX | CWY | CWWidth | CWHeight, &changes);
    return;
  }

  if ((e.state & Mod4Mask) &&
      (e.keycode == XKeysymToKeycode(_disp, XK_N)))
  {
    Window curFocus; int curRevert;
    XGetInputFocus(_disp, &curFocus, &curRevert);

    auto it = _clients.find(curFocus);
    if (it == end(_clients)) {
      LOG(ERROR) << "can't find client=" << e.subwindow;
      return;
    }
    auto& client = it->second;
    if (client.ign) return;

    if (client.preMax.w == 0 || client.preMax.h == 0) return;

    XWindowChanges changes;
    changes.x = client.preMax.o.x;
    changes.y = client.preMax.o.y;
    changes.width = client.preMax.w;
    changes.height = client.preMax.h;

    client.preMax.w = 0;
    client.preMax.h = 0;

    XConfigureWindow(_disp, client.client, CWX | CWY | CWWidth | CWHeight, &changes);
    return;
  }

  if ((e.state & Mod4Mask) &&
      (e.keycode == XKeysymToKeycode(_disp, XK_D)))
  {
    Window curFocus; int curRevert;
    XGetInputFocus(_disp, &curFocus, &curRevert);

    XEvent event;
    event.xclient.type = ClientMessage;
    event.xclient.window = curFocus;
    event.xclient.message_type = XInternAtom(_disp, "WM_PROTOCOLS", true);
    event.xclient.format = 32;
    event.xclient.data.l[0] = XInternAtom(_disp, "WM_DELETE_WINDOW", false);
    event.xclient.data.l[1] = CurrentTime;
    XSendEvent(_disp, curFocus, false, NoEventMask, &event);

    std::vector<std::pair<Rect, Window>> windows;
    for (auto& c : _clients)
      if (c.first != curFocus && !c.second.ign)
        windows.emplace_back(c.second.getRect(_disp), c.first);
    switchFocus(closestRectFromPoint(Rect(curFocus, _disp).getCenter(), windows));
    return;
  }
}

void Manager::onBtnPress(const XButtonEvent& e)
{
  LOG(INFO) << "btnPress window=" << e.window << " subwindow=" << e.subwindow << " " << e.state;

  // Normal click
  if (e.state == 0) {
    switchFocus(e.window);
    _drag.w = 0;
    return;
  }

  // Alt-click
  if (e.state & Mod4Mask) {
    switchFocus(e.window);

    _drag.btn = e.button;
    _drag.xR = e.x_root;
    _drag.yR = e.y_root;
    _drag.w = e.window;

    XWindowAttributes attr;
    XGetWindowAttributes(_disp, e.window, &attr);
    _drag.x = attr.x;
    _drag.y = attr.y;
    _drag.width = attr.width;
    _drag.height = attr.height;

    Point click(e.x_root, e.y_root);

    auto near = [] (int p2, int p1) -> bool { return std::max(0, p2 - p1) < 50; };

    _drag.dirHorz = DIR::Last;
    if (near(click.x, attr.x)) {
      _drag.dirHorz = DIR::Left;
    } else if (near(attr.x + attr.width, click.x)) {
      _drag.dirHorz = DIR::Right;
    }

    _drag.dirVert = DIR::Last;
    if (near(click.y, attr.y)) {
      _drag.dirVert = DIR::Up;
    } else if (near(attr.y + attr.height, click.y)) {
      _drag.dirVert = DIR::Down;
    }
  }
}

void Manager::switchFocus(Window w)
{
  Window curFocus; int curRevert;
  XGetInputFocus(_disp, &curFocus, &curRevert);

  if (curFocus == w)
    return;

  XSetInputFocus(_disp, w, RevertToPointerRoot, CurrentTime);
  XRaiseWindow(_disp, w);
}

void Manager::handleFocusChange(Window w, bool in)
{
  if (_screens.find(w) != end(_screens))
    return;
  if (!in) {
    LOG(INFO) << "regrab window=" << w;
    XGrabButton(_disp, 1, 0, w, false, ButtonPressMask,
                GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(_disp, 1, Mod2Mask, w, false, ButtonPressMask,
                GrabModeAsync, GrabModeAsync, None, None);
    XSetWindowBorderWidth(_disp, w, 2);
  } else {
    LOG(INFO) << "ungrab window=" << w;
    XUngrabButton(_disp, 1, 0, w);
    XSetWindowBorderWidth(_disp, w, 5);
  }
}

void Manager::addClient(Window w, bool checkIgn)
{
  auto it = _clients.find(w);
  if (it != end(_clients)) {
    LOG(ERROR) << "window=" << w << " is already framed!";
    return;
  }

  XWindowAttributes attrs;
  XGetWindowAttributes(_disp, w, &attrs);

  if (attrs.override_redirect)
    return; //TODO: Is this right?

  Client c;
  c.client = w;
  c.root = getRoot(w);
  c.ign = checkIgn && (attrs.override_redirect || (attrs.map_state != IsViewable));
  _clients.insert({w, c});

  // For selecting focus
  XGrabButton(_disp, 1, 0, w, false,
              ButtonPressMask | ButtonReleaseMask,
              GrabModeAsync, GrabModeAsync, None, None);
  XGrabButton(_disp, 1, Mod2Mask, w, false,
              ButtonPressMask | ButtonReleaseMask,
              GrabModeAsync, GrabModeAsync, None, None);

  // For moving/resizing
  XGrabButton(_disp, 1, Mod4Mask, w, false,
              ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
              GrabModeAsync, GrabModeAsync, None, None);
  XGrabButton(_disp, 1, Mod2Mask | Mod4Mask, w, false,
              ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
              GrabModeAsync, GrabModeAsync, None, None);
  XGrabButton(_disp, 3, Mod4Mask, w, false,
              ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
              GrabModeAsync, GrabModeAsync, None, None);
  XGrabButton(_disp, 3, Mod2Mask | Mod4Mask, w, false,
              ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
              GrabModeAsync, GrabModeAsync, None, None);

  XSelectInput(_disp, w, FocusChangeMask);

  XSetWindowBorder(_disp, w, 0x005F87);
  XSetWindowBorderWidth(_disp, w, 2);

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

Window Manager::getNextWindowInDir(DIR dir, Window w)
{
  if (dir == DIR::Last) return w;

  XWindowAttributes attr;
  XGetWindowAttributes(_disp, w, &attr);
  Point c = getCenter(attr.x, attr.y, attr.width, attr.height);

  Window closest = 0;
  int minDist = INT_MAX;

  for (const auto& m : _clients) {
    if (m.first == w)
      continue;
    if (m.second.ign)
      continue;

    XWindowAttributes a;
    XGetWindowAttributes(_disp, m.first, &a);
    Point o = getCenter(a.x, a.y, a.width, a.height);
    int plelDist = getDist(c, o, dir);
    if (plelDist == INT_MAX)
      continue; // Check on right side of centerline

    int perpDist;
    if (dir == DIR::Up || dir == DIR::Down) {
      perpDist = std::min(getDist(c, o, DIR::Left), getDist(c, o, DIR::Right));
    } else {
      perpDist = std::min(getDist(c, o, DIR::Up), getDist(c, o, DIR::Down));
    }
    if (perpDist == INT_MAX)
      continue;

    int dist = (int) sqrt((plelDist*plelDist) + 4*(perpDist*perpDist));
    if (dist < minDist) {
      minDist = dist;
      closest = m.first;
    }
  }

  if (closest == 0)
    return w;
  return closest;
}

template<typename T>
T Manager::closestRectFromPoint(const Point& p, std::vector<std::pair<Rect, T>>& rects)
{
  int minDist = INT_MAX;
  std::optional<T> closest;
  for (auto& r : rects) {
    Point o = r.first.getCenter();
    int vertDist = std::min(getDist(p, o, DIR::Up), getDist(p, o, DIR::Down));
    int horzDist = std::min(getDist(p, o, DIR::Left), getDist(p, o, DIR::Right));
    int dist = (int) sqrt((vertDist*vertDist) + (horzDist*horzDist));
    if (dist < minDist) {
      minDist = dist;
      closest = r.second;
    }
  }
  return closest.value_or(T());
}
