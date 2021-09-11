#include "Manager.hpp"

#include "Log.hpp"
#include "XUtils.hpp"

#include <algorithm>
#include <poll.h>
#include <set>
#include <string.h>

#define NUMLOCK (Mod2Mask)

#define BACKGROUND 0x604020

#define BORDER_THICK   5
#define BORDER_FOCUS   0x005F87
#define BORDER_UNFOCUS 0x0C0C0C

#define GRID_THICK 1
#define GRID_INACT 0x880000
#define GRID_COLOR 0x005F87
#define GRID_BG    0x181818

static constexpr int64_t DDC_POLL_INTERVAL = 60e9;

////////////////////////////////////////////////////////////////////////////////
/// Notes
///
/// - You may need to set Xcursor.size in ~/.Xresources
/// - Dependencies: pactl, slock, j4-dmenu-desktop, dmenu, st,
//                  import (imagemagick)

////////////////////////////////////////////////////////////////////////////////
/// Keyboard / Mouse Shortcuts
///
/// Numlock         + h,j,k,l | Move focus to other window
/// Numlock + Shift + h,j,k,l | Move window in grid on current monitor
/// Numlock + Ctrl  + h,j,k,l | Change window size using grid on current monitor
/// Numlock + Alt   + h,j,k,l | Move window to other monitors
///
/// Numlock + D   | Close the window that currently has focus
/// Numlock + T   | Open a terminal
/// Numlock + M   | Maximize the current window
/// Numlock + N   | Restore/unmaximize the current window
/// Numlock + G   | Open grid building mode
/// Numlock + S   | Snap current window to closest grid location / size
/// Numlock + Tab | TODO: Window explorer mode
/// Numlock + P   | Lock the screen
/// Numlock + A   | Open application menu launcher
/// Numlock + O   | Open screenshot dialog
/// Numlock + 1   | Switch monitor 1 input
/// Numlock + 2   | Switch monitor 1 input
/// Numlock + 3   | Switch monitor 2 input
/// Numlock + 4   | Switch monitor 2 input
/// Numlock + Q   | Volume up
/// Numlock + W   | Volume down
/// Numlock + E   | Volume toggle mute
///
/// Grid Building Mode
/// j,k             | Decrement/increment vertical grid count
/// h,l             | Decrement/increment horizontal grid count
/// Shift + h,j,k,l | Move focus to other monitor

Manager::Manager(const std::string& display,
                 const std::map<int,Point>& screens,
                 const std::string& screenshotDir,
                 const std::map<std::string,MonitorCfg>& monitorCfg)
  : _argDisp(display)
  , _argScreens(screens)
  , _argScreenshotDir(screenshotDir)
  , _argMonitorCfg(monitorCfg)
{}

Manager::~Manager()
{
  if (_disp != nullptr) {
    XCloseDisplay(_disp);
    _disp = nullptr;
  }
}

bool Manager::init()
{
  system("pactl upload-sample /usr/share/sounds/freedesktop/stereo/bell.oga bell.oga");

  std::vector<DDCDisplayId> expectedMonitors;
  for (const auto& [_,mon] : _argMonitorCfg)
    expectedMonitors.push_back(mon.id);
  if (!_ddc.init(expectedMonitors))
    return false;

  XSetErrorHandler(&XError);

  _disp = XOpenDisplay(_argDisp.c_str());
  if (_disp == nullptr) {
    LOG(ERROR) << "failed to open X display=" << _argDisp;
    return false;
  }

  auto numScreens = ScreenCount(_disp);
  LOG(INFO) << "display=" << DisplayString(_disp) << " screens=" << numScreens;

  for (int i = 0; i < numScreens; ++i) {
    if (_argScreens.find(i) == _argScreens.end())
      continue;

    auto root = RootWindow(_disp, i);
    LOG(INFO) << "screen=" << DisplayString(_disp) << "." << i << " root=" << root
              << " origin=(" << _argScreens.at(i).x << "," << _argScreens.at(i).y << ")";
    Root r;
    r.screen = i;
    r.absOrigin = _argScreens.at(i);
    _roots[root] = r;

    XSelectInput(_disp, root, SubstructureRedirectMask | SubstructureNotifyMask |
                              KeyPressMask | ButtonPressMask | FocusChangeMask);

    // Set the background
    XSetWindowBackground(_disp, root, BACKGROUND);
    XClearWindow(_disp, root);

    // Less ugly cursor
    Cursor cursor = XCreateFontCursor(_disp, XC_crosshair);
    XDefineCursor(_disp, root, cursor);

    // Identify monitors on this X screen
    {
      bool success = true;

      auto* res = XRRGetScreenResources(_disp, root);
      assert(res != nullptr);
      for (int j = 0; j < res->ncrtc && success; ++j) {
        auto* crtc = XRRGetCrtcInfo(_disp, res, res->crtcs[j]);
        assert(crtc != nullptr);
        Rect rect{crtc->x, crtc->y, int(crtc->width), int(crtc->height)};
        for (int k = 0; k < crtc->noutput && success; ++k) {
          auto* output = XRRGetOutputInfo(_disp, res, crtc->outputs[k]);
          assert(output != nullptr);
          std::string_view connector(output->name, output->nameLen);

          auto it = std::find_if(_argMonitorCfg.begin(), _argMonitorCfg.end(),
              [&] (const auto& m) { return m.second.screen == i && m.second.connector == connector; });
          if (it == _argMonitorCfg.end()) {
            LOG(ERROR) << "missing config for monitor screen=" << i << " connector=(" << connector << ")";
            success = false;
          } else {
            auto jt = std::find_if(_monitors.begin(), _monitors.end(),
                [&] (const auto& mon) { return mon.cfg.name == it->second.name; });
            if (jt != _monitors.end()) {
              LOG(ERROR) << "duplicate monitor config screen=" << i
                         << " connector=(" << connector << ")"
                         << " name=(" << it->second.name << ")";
              success = false;
            } else {
              LOG(INFO) << "found monitor"
                        << " name=(" << it->second.name << ")"
                        << " screen=" << i
                        << " connector=(" << connector << ")"
                        << " width=" << rect.w
                        << " height=" << rect.h
                        << " xPos=" << rect.o.x
                        << " yPos=" << rect.o.y;
              _monitors.emplace_back(Monitor{it->second, rect, root, _argScreens.at(i), std::nullopt, 0, 1, 1});
            }
          }
          XRRFreeOutputInfo(output);
        }
        XRRFreeCrtcInfo(crtc);
      }
      XRRFreeScreenResources(res);

      if (!success) {
        LOG(ERROR) << "unable to identify monitors on this screen=" << i;
        return false;
      }
    }

    // Add pre-existing windows on this screen
    XGrabServer(_disp);
    Window root2, parent; Window* children; uint32_t num;
    XQueryTree(_disp, root, &root2, &parent, &children, &num);
    for (uint32_t j = 0; j < num; ++j)
      addClient(children[j], true);
    XFree(children);
    XUngrabServer(_disp);
  }

  if (_monitors.size() != _argMonitorCfg.size()) {
    LOG(ERROR) << "did not detect all configured monitors";
    return false;
  }

  return true;
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

  if (attrs.c_class == InputOnly || attrs.override_redirect) {
    LOG(WARN) << "ignoring non-graphics window=" << w;
    return;
  }

  Client c;
  c.client = w;
  c.root = GetWinRoot(_disp, w);
  c.ign = checkIgn && (attrs.override_redirect || (attrs.map_state != IsViewable));
  c.absOrigin = _roots.at(c.root).absOrigin;
  _clients.insert({w, c});

  // For selecting focus
  XGrabButton(_disp, 1, 0, w, false,
              ButtonPressMask,
              GrabModeSync, GrabModeAsync, None, None);

  // For moving/resizing
  XGrabButton(_disp, 1, NUMLOCK, w, false,
              ButtonPressMask | ButtonMotionMask,
              GrabModeAsync, GrabModeAsync, None, None);
  XGrabButton(_disp, 3, NUMLOCK, w, false,
              ButtonPressMask | ButtonMotionMask,
              GrabModeAsync, GrabModeAsync, None, None);

  // Grab keys with NUMLOCK modifier
  static const std::set<int> KEYS = {
    XK_Tab, XK_D, XK_T, XK_M, XK_N, XK_G, XK_S, XK_P, XK_A, XK_O,
    XK_1, XK_2, XK_3, XK_4, XK_Q, XK_W, XK_E
  };
  static const std::set<int> MOV_KEYS = { XK_H, XK_J, XK_K, XK_L };
  for (int key : KEYS)
    XGrabKey(_disp, XKeysymToKeycode(_disp, key), NUMLOCK, w, false, GrabModeAsync, GrabModeAsync);
  for (int key : MOV_KEYS) {
    XGrabKey(_disp, XKeysymToKeycode(_disp, key), NUMLOCK, w, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, key), NUMLOCK | ShiftMask, w, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, key), NUMLOCK | ControlMask, w, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(_disp, XKeysymToKeycode(_disp, key), NUMLOCK | Mod1Mask /*Alt*/, w, false, GrabModeAsync, GrabModeAsync);
  }

  XGrabKey(_disp, XKeysymToKeycode(_disp, XF86XK_AudioMute), AnyModifier, w, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(_disp, XKeysymToKeycode(_disp, XF86XK_AudioRaiseVolume), AnyModifier, w, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(_disp, XKeysymToKeycode(_disp, XF86XK_AudioLowerVolume), AnyModifier, w, false, GrabModeAsync, GrabModeAsync);

  XSelectInput(_disp, w, FocusChangeMask);

  XSetWindowBorderWidth(_disp, w, BORDER_THICK);
  XSetWindowBorder(_disp, w, BORDER_UNFOCUS);

  // Check to make sure we dont place a new client somewhere off the screens
  if (std::find_if(begin(_monitors), end(_monitors),
        [&] (const auto& m) { return m.root == c.root && m.r.contains(Point(attrs.x, attrs.y)); }) ==
      end(_monitors))
  {
    LOG(INFO) << "new client started off the screen, relocating client=" << w;

    std::vector<std::pair<Rect, Monitor*>> monitors;
    for (auto& m : _monitors)
      if (c.root == m.root)
        monitors.emplace_back(m.r, &m);
    auto* mon = closestRectFromPoint(Point(attrs.x, attrs.y), monitors);

    int curW = std::min(attrs.width + (2 * BORDER_THICK), mon->r.w);
    int curH = std::min(attrs.height + (2 * BORDER_THICK), mon->r.h);
    XWindowChanges changes;
    changes.x = mon->r.getCenter().x - (curW / 2);
    changes.y = mon->r.getCenter().y - (curH / 2);
    changes.width = curW - (2 * BORDER_THICK);
    changes.height = curH - (2 * BORDER_THICK);
    XConfigureWindow(_disp, w, (CWX | CWY | CWWidth | CWHeight), &changes);
  }

  XMapWindow(_disp, w);
  LOG(INFO) << "added client=" << w;
}

void Manager::run()
{
  struct pollfd pfd;
  pfd.fd = ConnectionNumber(_disp);
  pfd.events = POLLIN;

  // Main event loop
  for (;;) {
    ::poll(&pfd, 1, 10); // Sleep until there are events or until the timeout
    const int64_t now = getTime();

    if (now - _lastDdcPoll > DDC_POLL_INTERVAL)
      pollDdc(now);

    while (XPending(_disp)) {
      XEvent e;
      XNextEvent(_disp, &e);
      //LOG(INFO) << "type=" << XEventToString(e) << " serial=" << e.xany.serial << " pending=" << XPending(_disp);

      switch (e.type) {
        // Ignore these events
        case ReparentNotify:
        case MapNotify:
        case MappingNotify:
        case ConfigureNotify:
        case CreateNotify:
        case DestroyNotify:
        case KeyRelease:
          break;

        case MapRequest:
          onReq_Map(e.xmaprequest);
          break;
        case UnmapNotify:
          onNot_Unmap(e.xunmap);
          break;
        case ConfigureRequest:
          onReq_Configure(e.xconfigurerequest);
          break;

        case MotionNotify:
          while (XCheckTypedWindowEvent(_disp, e.xmotion.window, MotionNotify, &e)); // Get latest
          onNot_Motion(e.xbutton);
          break;

        case FocusIn:
          handleFocusChange(e.xfocus, true);
          break;
        case FocusOut:
          handleFocusChange(e.xfocus, false);
          break;

        case KeyPress:
          onKeyPress(e.xkey);
          break;
        case ButtonPress:
          onBtnPress(e.xbutton);
          break;

        case ClientMessage:
          onClientMessage(e.xclient);
          break;

        default:
          LOG(ERROR) << "XEvent not yet handled type=" << e.type
                     << " event=(" << XEventToString(e) << ")";
          break;
      }
    }
  }
}

/// X Event Handlers ///////////////////////////////////////////////////////////

void Manager::onReq_Map(const XMapRequestEvent& e)
{
  LOG(INFO) << "request=Map window=" << e.window;

  if (e.serial <= _lastMapSerial) {
    LOG(WARN) << "ignoring repeated map request window=" << e.window;
    return;
  }

  addClient(e.window, false);

  _lastMapSerial = e.serial;
}

void Manager::onNot_Unmap(const XUnmapEvent& e)
{
  LOG(INFO) << "notify=Unmap window=" << e.window;

  auto it = _clients.find(e.window);
  if (it != end(_clients)) {
    _clients.erase(it);
    LOG(INFO) << "deleted client=" << e.window;
  }
}

void Manager::onReq_Configure(const XConfigureRequestEvent& e)
{
  LOG(INFO) << "request=Configure window=" << e.window;

  if (e.serial <= _lastConfigureSerial) {
    LOG(WARN) << "ignoring repeated configure request window=" << e.window;
    return;
  }

  XWindowChanges changes;
  bzero(&changes, sizeof(changes));

  unsigned changeMask = 0;

  if (e.value_mask & CWX) {
    changes.x = e.x;
    changeMask |= CWX;
  }
  if (e.value_mask & CWY) {
    changes.y = e.y;
    changeMask |= CWY;
  }
  if (e.value_mask & CWWidth) {
    changes.width = e.width;
    changeMask |= CWWidth;
  }
  if (e.value_mask & CWHeight) {
    changes.height = e.height;
    changeMask |= CWHeight;
  }

  XConfigureWindow(_disp, e.window, changeMask, &changes);

  _lastConfigureSerial = e.serial;
}

void Manager::onNot_Motion(const XButtonEvent& e)
{
  if (_drag.w == 0)
    return;

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
  LOG(INFO) << "keyPress"
            << " window=" << e.window
            << " subwindow=" << e.subwindow
            << " keyCode=" << e.keycode
            << " state=" << e.state;

  if (_gridActive) {
    onKeyGridActive(e);
    return;
  }

  if (e.keycode == XKeysymToKeycode(_disp, XF86XK_AudioMute)) {
    system("pactl set-sink-mute @DEFAULT_SINK@ toggle");
    system("pactl play-sample bell.oga");
    return;
  }
  if (e.keycode == XKeysymToKeycode(_disp, XF86XK_AudioRaiseVolume)) {
    system("pactl set-sink-volume @DEFAULT_SINK@ +1000");
    system("pactl set-sink-mute @DEFAULT_SINK@ 0");
    system("pactl play-sample bell.oga");
    return;
  }
  if (e.keycode == XKeysymToKeycode(_disp, XF86XK_AudioLowerVolume)) {
    system("pactl set-sink-volume @DEFAULT_SINK@ -1000");
    system("pactl set-sink-mute @DEFAULT_SINK@ 0");
    system("pactl play-sample bell.oga");
    return;
  }

  if (!(e.state & NUMLOCK)) {
    if (_roots.find(e.window) == _roots.end())
      LOG(ERROR) << "captured non-modifier keyPress keyCode=" << e.keycode;
    return;
  }

  if (e.keycode == XKeysymToKeycode(_disp, XK_Tab))
    onKeyWinExplorer(e);
  else if (e.keycode == XKeysymToKeycode(_disp, XK_T))
    onKeyTerminal(e);
  else if (e.keycode == XKeysymToKeycode(_disp, XK_G))
    onKeyGrid(e);
  else if (e.keycode == XKeysymToKeycode(_disp, XK_S))
    onKeySnapGrid(e);
  else if (e.keycode == XKeysymToKeycode(_disp, XK_H) ||
           e.keycode == XKeysymToKeycode(_disp, XK_J) ||
           e.keycode == XKeysymToKeycode(_disp, XK_K) ||
           e.keycode == XKeysymToKeycode(_disp, XK_L)) {
    if (e.state & ShiftMask)
      onKeyMoveGridLoc(e);
    else if (e.state & ControlMask)
      onKeyMoveGridSize(e);
    else if (e.state & Mod1Mask)
      onKeyMoveMonitor(e);
    else
      onKeyMoveFocus(e);
  }
  else if (e.keycode == XKeysymToKeycode(_disp, XK_M))
    onKeyMaximize(e);
  else if (e.keycode == XKeysymToKeycode(_disp, XK_N))
    onKeyUnmaximize(e);
  else if (e.keycode == XKeysymToKeycode(_disp, XK_D))
    onKeyClose(e);
  else if (e.keycode == XKeysymToKeycode(_disp, XK_P))
    system("slock");
  else if (e.keycode == XKeysymToKeycode(_disp, XK_A))
    onKeyLauncher(e);
  else if (e.keycode == XKeysymToKeycode(_disp, XK_O))
    onKeyScreenshot(e);
  else if (e.keycode == XKeysymToKeycode(_disp, XK_1) ||
           e.keycode == XKeysymToKeycode(_disp, XK_2) ||
           e.keycode == XKeysymToKeycode(_disp, XK_3) ||
           e.keycode == XKeysymToKeycode(_disp, XK_4))
    onKeyMonitorInput(e);
  else if (e.keycode == XKeysymToKeycode(_disp, XK_Q)) {
    system("pactl set-sink-volume @DEFAULT_SINK@ +1000");
    system("pactl set-sink-mute @DEFAULT_SINK@ 0");
    system("pactl play-sample bell.oga");
  }
  else if (e.keycode == XKeysymToKeycode(_disp, XK_W)) {
    system("pactl set-sink-volume @DEFAULT_SINK@ -1000");
    system("pactl set-sink-mute @DEFAULT_SINK@ 0");
    system("pactl play-sample bell.oga");
  }
  else if (e.keycode == XKeysymToKeycode(_disp, XK_E)) {
    system("pactl set-sink-mute @DEFAULT_SINK@ toggle");
    system("pactl play-sample bell.oga");
  }
  else {
    if (_roots.find(e.window) == _roots.end())
      LOG(ERROR) << "unhandled keyPress keyCode=" << e.keycode;
  }
}

void Manager::onBtnPress(const XButtonEvent& e)
{
  LOG(INFO) << "btnPress"
            << " window=" << e.window
            << " subwindow=" << e.subwindow
            << " button=" << e.button
            << " state=" << e.state;

  // Normal click
  if (e.state == 0) {
    switchFocus(e.window);
    _drag = {};
    XAllowEvents(_disp, ReplayPointer, CurrentTime); // Replay button click so client handles it
    return;
  }

  if (_gridActive)
    return;

  // Alt-click
  if (e.state & NUMLOCK) {
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

    _drag.dirHorz = DIR::LAST;
    if (near(click.x, attr.x)) {
      _drag.dirHorz = DIR::Left;
    } else if (near(attr.x + attr.width, click.x)) {
      _drag.dirHorz = DIR::Right;
    }

    _drag.dirVert = DIR::LAST;
    if (near(click.y, attr.y)) {
      _drag.dirVert = DIR::Up;
    } else if (near(attr.y + attr.height, click.y)) {
      _drag.dirVert = DIR::Down;
    }
  }
}

//TODO: Better handling of different ClientMessage types
void Manager::onClientMessage(const XClientMessageEvent& e)
{
  char* str = XGetAtomName(e.display, e.message_type);
  LOG(INFO) << "clientMessage"
            << " window=" << e.window
            << " serial=" << e.serial
            << " send_event=" << e.send_event
            << " display=" << e.display
            << " message_type=" << e.message_type
            << " format=" << e.format
            << " data=(" << e.data.b << ")"
            << " atom=(" << str << ")";
  XFree(str);

  LOG(INFO) << e.data.l[0] << " " << e.data.l[1] << " "
            << e.data.l[2] << " " << e.data.l[3] << " "
            << XGetAtomName(e.display, e.data.l[1]);
}

/// Focus Handlers /////////////////////////////////////////////////////////////

void Manager::switchFocus(Window w)
{
  Window curFocus; int curRevert;
  XGetInputFocus(_disp, &curFocus, &curRevert);

  if (curFocus == w)
    return;

  LOG(INFO) << "switching focus from current=" << curFocus << " new=" << w;
  XSetInputFocus(_disp, w, RevertToPointerRoot, CurrentTime);
  XRaiseWindow(_disp, w);
}

void Manager::handleFocusChange(const XFocusChangeEvent& e, bool in)
{
  if (e.mode == NotifyGrab || e.mode == NotifyUngrab)
    return;

  auto it = std::find_if(begin(_monitors), end(_monitors),
      [&] (const auto& m) { return m.gridDraw == e.window; });
  if (it != end(_monitors)) {
    drawGrid(&(*it), in);
    return;
  }

  if (_clients.find(e.window) == end(_clients))
      return;

  if (!in) {
    LOG(INFO) << "focus out, regrab window=" << e.window;
    XGrabButton(_disp, 1, 0, e.window, false, ButtonPressMask,
                GrabModeSync, GrabModeAsync, None, None);
    XSetWindowBorder(_disp, e.window, BORDER_UNFOCUS);
  } else {
    LOG(INFO) << "focus in, ungrab window=" << e.window;
    XUngrabButton(_disp, 1, 0, e.window);
    XSetWindowBorder(_disp, e.window, BORDER_FOCUS);
    _lastFocus = e.window;
  }
}

/// Key Press Handlers /////////////////////////////////////////////////////////

void Manager::onKeyGridActive(const XKeyEvent& e)
{
  if (e.keycode == XKeysymToKeycode(_disp, XK_G)) {
    _gridActive = false;
    switchFocus(_lastFocus);
    for (const auto& monitor : _monitors)
      XUnmapWindow(_disp, monitor.gridDraw);
    return;
  }

  auto it = std::find_if(begin(_monitors), end(_monitors),
      [&] (const Monitor& m) { return m.gridDraw == e.window; });
  if (it == end(_monitors)) {
    LOG(ERROR) << "invalid window keypress in grid build mode window=" << e.window;
    return;
  }
  auto* mon = &(*it);

  if (e.keycode == XKeysymToKeycode(_disp, XK_H) ||
      e.keycode == XKeysymToKeycode(_disp, XK_J) ||
      e.keycode == XKeysymToKeycode(_disp, XK_K) ||
      e.keycode == XKeysymToKeycode(_disp, XK_L)) {
    if (e.state & ShiftMask) {
      DIR dir;
      if (e.keycode == XKeysymToKeycode(_disp, XK_H))
        dir = DIR::Left;
      else if (e.keycode == XKeysymToKeycode(_disp, XK_J))
        dir = DIR::Down;
      else if (e.keycode == XKeysymToKeycode(_disp, XK_K))
        dir = DIR::Up;
      else if (e.keycode == XKeysymToKeycode(_disp, XK_L))
        dir = DIR::Right;

      std::vector<std::pair<Point, Monitor*>> monitors;
      for (auto& monitor : _monitors)
        if (&monitor != mon)
          monitors.emplace_back(monitor.absOrigin + monitor.r.getCenter(), &monitor);
      if (auto* m = getNextPointInDir(dir, mon->absOrigin + mon->r.getCenter(), monitors); m != nullptr)
        switchFocus(m->gridDraw);
    } else {
      if (e.keycode == XKeysymToKeycode(_disp, XK_J))
        mon->gridY = (mon->gridY == 1) ? 1 : mon->gridY - 1;
      else if (e.keycode == XKeysymToKeycode(_disp, XK_K))
        mon->gridY++;
      else if (e.keycode == XKeysymToKeycode(_disp, XK_H))
        mon->gridX = (mon->gridX == 1) ? 1 : mon->gridX - 1;
      else if (e.keycode == XKeysymToKeycode(_disp, XK_L))
        mon->gridX++;
      drawGrid(mon, true);
    }
    return;
  }

  LOG(ERROR) << "invalid window keypress in grid build mode keycode=" << e.keycode;
}

void Manager::onKeyWinExplorer(const XKeyEvent& /*e*/)
{
  // Calculate new coordinates for all clients on a monitor
  // Save old position of each client and move them to new coordinates
  // On click on a client, send all *other* clients back to their old position
}

void Manager::onKeyTerminal(const XKeyEvent& e)
{
  LOG(INFO) << "launching terminal window=" << e.window;

  Window root = GetWinRoot(_disp, e.window);
  int screen = _roots.at(root).screen;

  static const int cols = 120;
  static const int rows = 40;
  static const int offset = 100;
  int x = offset;
  int y = offset;

  XWindowAttributes attr;
  XGetWindowAttributes(_disp, e.window, &attr);
  Rect cur(attr.x, attr.y, attr.width, attr.height);
  Point cen = cur.getCenter();
  auto itm = std::find_if(begin(_monitors), end(_monitors),
                          [&] (const auto& m) { return m.root == root && m.r.contains(cen); });
  if (itm == end(_monitors)) {
    LOG(ERROR) << "no monitor contains (" << cen.x << "," << cen.y << ")";
  } else {
    x = itm->r.o.x + offset;
    y = itm->r.o.y + offset;
  }

  std::ostringstream cmd;
  cmd << "DISPLAY=" << DisplayString(_disp) << "." << screen << " ";
  cmd << "st -g " << cols << "x" << rows << "+" << x << "+" << y << " &";
  LOG(INFO) << "starting cmd=(" << cmd.str() << ")";
  system(cmd.str().c_str());
}

void Manager::onKeyGrid(const XKeyEvent& /*e*/)
{
  LOG(INFO) << "activating grid building mode";

  _gridActive = true;

  for (auto& monitor : _monitors) {
    auto gridDraw = XCreateSimpleWindow(_disp, monitor.root,
        monitor.r.o.x, monitor.r.o.y,
        monitor.r.w - 2*GRID_THICK, monitor.r.h - 2*GRID_THICK,
        GRID_THICK, GRID_COLOR, GRID_BG);
    monitor.gridDraw = gridDraw;

    static const std::set<int> KEYS = { XK_H, XK_J, XK_K, XK_L, XK_G };
    for (int key : KEYS) {
      XGrabKey(_disp, XKeysymToKeycode(_disp, key), 0, gridDraw, false, GrabModeAsync, GrabModeAsync);
      XGrabKey(_disp, XKeysymToKeycode(_disp, key), ShiftMask, gridDraw, false, GrabModeAsync, GrabModeAsync);
    }

    XSelectInput(_disp, gridDraw, FocusChangeMask);
    XMapWindow(_disp, gridDraw);
    switchFocus(gridDraw);
  }
}

void Manager::onKeyMoveMonitor(const XKeyEvent& e)
{
  DIR dir;
  if (e.keycode == XKeysymToKeycode(_disp, XK_H))
    dir = DIR::Left;
  else if (e.keycode == XKeysymToKeycode(_disp, XK_J))
    dir = DIR::Down;
  else if (e.keycode == XKeysymToKeycode(_disp, XK_K))
    dir = DIR::Up;
  else if (e.keycode == XKeysymToKeycode(_disp, XK_L))
    dir = DIR::Right;

  XWindowAttributes attr;
  XGetWindowAttributes(_disp, e.window, &attr);
  Rect cur(attr.x, attr.y, attr.width, attr.height);
  Point cen = cur.getCenter();
  Window root = GetWinRoot(_disp, e.window);

  auto it = std::find_if(begin(_monitors), end(_monitors),
                         [&] (const auto& m) { return m.root == root && m.r.contains(cen); });
  if (it == end(_monitors)) {
    LOG(ERROR) << "no monitor contains (" << cen.x << "," << cen.y << ")";
    return;
  }
  auto* curMon = &(*it);

  std::vector<std::pair<Point, Monitor*>> monitors;
  for (auto& monitor : _monitors)
    if (&monitor != curMon && monitor.root == root)
      monitors.emplace_back(monitor.r.getCenter(), &monitor);
  auto* m = getNextPointInDir(dir, curMon->r.getCenter(), monitors);
  if (m == nullptr)
    return;

  int w = std::min(cur.w + (2 * BORDER_THICK), m->r.w);
  int h = std::min(cur.h + (2 * BORDER_THICK), m->r.h);

  XWindowChanges changes;
  changes.x = m->r.getCenter().x - (w / 2);
  changes.y = m->r.getCenter().y - (h / 2);
  changes.width = w - (2 * BORDER_THICK);
  changes.height = h - (2 * BORDER_THICK);

  XConfigureWindow(_disp, e.window, (CWX | CWY | CWWidth | CWHeight), &changes);
}

void Manager::onKeyMoveFocus(const XKeyEvent& e)
{
  DIR dir;
  if (e.keycode == XKeysymToKeycode(_disp, XK_H))
    dir = DIR::Left;
  else if (e.keycode == XKeysymToKeycode(_disp, XK_J))
    dir = DIR::Down;
  else if (e.keycode == XKeysymToKeycode(_disp, XK_K))
    dir = DIR::Up;
  else if (e.keycode == XKeysymToKeycode(_disp, XK_L))
    dir = DIR::Right;

  Window curFocus; int curRevert;
  XGetInputFocus(_disp, &curFocus, &curRevert);
  if (curFocus == PointerRoot || curFocus == None)
    curFocus = _roots.begin()->first;
  else if (_clients.find(curFocus) == end(_clients))
    curFocus = GetWinRoot(_disp, curFocus);

  Window nextFocus = getNextWindowInDir(dir, curFocus);
  switchFocus(nextFocus);
}

void Manager::onKeyMaximize(const XKeyEvent& e)
{
  Window curFocus; int curRevert;
  XGetInputFocus(_disp, &curFocus, &curRevert);

  auto it = _clients.find(curFocus);
  if (it == end(_clients)) {
    LOG(ERROR) << "unable to find client=" << e.subwindow;
    return;
  }
  auto& client = it->second;
  if (client.ign)
    return;

  LOG(INFO) << "maximizing"
            << " curFocus=" << curFocus
            << " window=" << e.window
            << " subwindow=" << e.subwindow;

  XWindowAttributes attr;
  XGetWindowAttributes(_disp, client.client, &attr);
  Point c = Rect(attr.x, attr.y, attr.width, attr.height).getCenter();

  auto it2 = std::find_if(begin(_monitors), end(_monitors),
                          [&] (const auto& m) { return m.root == client.root && m.r.contains(c); });
  if (it2 == end(_monitors)) {
    LOG(ERROR) << "no monitor contains (" << c.x << "," << c.y << ")";
    return;
  }
  auto& mon = *it2;

  auto nw = mon.r.w - (2 * BORDER_THICK);
  auto nh = mon.r.h - (2 * BORDER_THICK);

  if (attr.width == nw && attr.height == nh)
    return;

  client.preMax.o.x = attr.x;
  client.preMax.o.y = attr.y;
  client.preMax.w = attr.width;
  client.preMax.h = attr.height;

  XWindowChanges changes;
  changes.x = mon.r.o.x;
  changes.y = mon.r.o.y;
  changes.width = nw;
  changes.height = nh;
  XConfigureWindow(_disp, client.client, (CWX | CWY | CWWidth | CWHeight), &changes);
}

void Manager::onKeyUnmaximize(const XKeyEvent& e)
{
  Window curFocus; int curRevert;
  XGetInputFocus(_disp, &curFocus, &curRevert);

  auto it = _clients.find(curFocus);
  if (it == end(_clients)) {
    LOG(ERROR) << "unable to find client=" << e.subwindow;
    return;
  }
  auto& client = it->second;
  if (client.ign)
    return;

  LOG(INFO) << "unmaximizing"
            << " curFocus=" << curFocus
            << " window=" << e.window
            << " subwindow=" << e.subwindow;

  if (client.preMax.w == 0 || client.preMax.h == 0)
    return;

  XWindowChanges changes;
  changes.x = client.preMax.o.x;
  changes.y = client.preMax.o.y;
  changes.width = client.preMax.w;
  changes.height = client.preMax.h;

  client.preMax.w = 0;
  client.preMax.h = 0;

  XConfigureWindow(_disp, client.client, (CWX | CWY | CWWidth | CWHeight), &changes);
}

void Manager::onKeyClose(const XKeyEvent& e)
{
  Window curFocus; int curRevert;
  XGetInputFocus(_disp, &curFocus, &curRevert);
  auto center = _roots.at(GetWinRoot(_disp, curFocus)).absOrigin + GetWinRect(_disp, curFocus).getCenter();

  LOG(INFO) << "closing window"
            << " curFocus=" << curFocus
            << " window=" << e.window
            << " subwindow=" << e.subwindow;

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
      windows.emplace_back(GetWinRect(_disp, c.first) + c.second.absOrigin, c.first);
  switchFocus(closestRectFromPoint(center, windows));
}

void Manager::onKeyLauncher(const XKeyEvent& e)
{
  int screen = _roots.at(GetWinRoot(_disp, e.window)).screen;
  std::ostringstream cmd;
  cmd << "DISPLAY=" << DisplayString(_disp) << "." << screen << " ";
  cmd << "j4-dmenu-desktop";
  cmd << " --dmenu=\"dmenu -i -p 'mwm' -l 25 -c -w " << e.window << "\"";
  cmd << " --term=\"st\"";
  cmd << " >/dev/null 2>&1 &";
  system(cmd.str().c_str());
}

void Manager::onKeyScreenshot(const XKeyEvent& e)
{
  int screen = _roots.at(GetWinRoot(_disp, e.window)).screen;
  std::ostringstream cmd;
  cmd << "DISPLAY=" << DisplayString(_disp) << "." << screen << " ";
  cmd << "import \"" << _argScreenshotDir << "/screenshot-$(date '+%Y-%m-%d::%H:%M:%S').png\" &";
  LOG(INFO) << "starting cmd=(" << cmd.str() << ")";
  system(cmd.str().c_str());
}

void Manager::snapGrid(Window w, Rect r)
{
  Point c = r.getCenter();
  Window root = GetWinRoot(_disp, w);
  auto it = std::find_if(begin(_monitors), end(_monitors),
                         [&] (const auto& m) { return m.root == root && m.r.contains(c); });
  if (it == end(_monitors)) {
    LOG(ERROR) << "no monitor contains (" << c.x << "," << c.y << ")";
    return;
  }
  auto& mon = *it;

  double gridW = double(mon.r.w) / mon.gridX;
  double gridH = double(mon.r.h) / mon.gridY;
  int xNum = std::max(1, (int) round(double(r.w) / gridW));
  int yNum = std::max(1, (int) round(double(r.h) / gridH));
  auto widX = int(xNum * gridW);
  auto widY = int(yNum * gridH);

  int minX;
  int minDist = INT_MAX;
  for (unsigned j = 1, k = xNum; j <= (mon.gridX - xNum) + 1; ++j, k += 2) {
    auto gridCen = mon.r.o.x + int(k*(gridW/2));
    auto dist = abs(c.x - gridCen);
    if (dist < minDist) {
      minDist = dist;
      minX = gridCen;
    }
  }

  int minY;
  minDist = INT_MAX;
  for (unsigned j = 1, k = yNum; j <= (mon.gridY - yNum) + 1; ++j, k += 2) {
    auto gridCen = mon.r.o.y + int(k*(gridH/2));
    auto dist = abs(c.y - gridCen);
    if (dist < minDist) {
      minDist = dist;
      minY = gridCen;
    }
  }

  XWindowChanges changes;
  changes.width = widX - (2 * BORDER_THICK);
  changes.height = widY - (2 * BORDER_THICK);
  changes.x = minX - (widX / 2);
  changes.y = minY - (widY / 2);
  XConfigureWindow(_disp, w, (CWX | CWY | CWWidth | CWHeight), &changes);
}

void Manager::onKeySnapGrid(const XKeyEvent& e)
{
  XWindowAttributes attr;
  XGetWindowAttributes(_disp, e.window, &attr);
  snapGrid(e.window, Rect(attr.x, attr.y, attr.width, attr.height));
}

void Manager::onKeyMoveGridLoc(const XKeyEvent& e)
{
  XWindowAttributes attr;
  XGetWindowAttributes(_disp, e.window, &attr);
  Rect loc(attr.x, attr.y, attr.width, attr.height);
  Point c = loc.getCenter();
  Window root = GetWinRoot(_disp, e.window);

  auto it = std::find_if(begin(_monitors), end(_monitors),
                         [&] (const auto& m) { return m.root == root && m.r.contains(c); });
  if (it == end(_monitors)) {
    LOG(ERROR) << "no monitor contains (" << c.x << "," << c.y << ")";
    return;
  }
  auto& mon = *it;

  int gridW = mon.r.w / mon.gridX;
  int gridH = mon.r.h / mon.gridY;

  if (e.keycode == XKeysymToKeycode(_disp, XK_H))
    loc.o.x = std::max(loc.o.x - gridW, mon.r.o.x);
  else if (e.keycode == XKeysymToKeycode(_disp, XK_J))
    loc.o.y = std::min(loc.o.y + gridH, mon.r.o.y + mon.r.h - loc.h);
  else if (e.keycode == XKeysymToKeycode(_disp, XK_K))
    loc.o.y = std::max(loc.o.y - gridH, mon.r.o.y);
  else if (e.keycode == XKeysymToKeycode(_disp, XK_L))
    loc.o.x = std::min(loc.o.x + gridW, mon.r.o.x + mon.r.w - loc.w);

  snapGrid(e.window, loc);
}

void Manager::onKeyMoveGridSize(const XKeyEvent& e)
{
  XWindowAttributes attr;
  XGetWindowAttributes(_disp, e.window, &attr);
  Rect loc(attr.x, attr.y, attr.width, attr.height);
  Point c = loc.getCenter();
  Window root = GetWinRoot(_disp, e.window);

  auto it = std::find_if(begin(_monitors), end(_monitors),
                         [&] (const auto& m) { return m.root == root && m.r.contains(c); });
  if (it == end(_monitors)) {
    LOG(ERROR) << "no monitor contains (" << c.x << "," << c.y << ")";
    return;
  }
  auto& mon = *it;

  int gridW = mon.r.w / mon.gridX;
  int gridH = mon.r.h / mon.gridY;

  if (e.keycode == XKeysymToKeycode(_disp, XK_H))
    loc.w = std::max(loc.w - gridW, gridW);
  else if (e.keycode == XKeysymToKeycode(_disp, XK_J))
    loc.h = std::max(loc.h - gridH, gridH);
  else if (e.keycode == XKeysymToKeycode(_disp, XK_K))
    loc.h = std::min(loc.h + gridH, mon.r.h);
  else if (e.keycode == XKeysymToKeycode(_disp, XK_L))
    loc.w = std::min(loc.w + gridW, mon.r.w);

  snapGrid(e.window, loc);
}

void Manager::onKeyMonitorInput(const XKeyEvent& e)
{
  const DDCDisplayId* id;
  uint8_t source;
  if (e.keycode == XKeysymToKeycode(_disp, XK_1)) {
    id = &(_argMonitorCfg.at("Left").id);
    source = 0x1b;
  } else if (e.keycode == XKeysymToKeycode(_disp, XK_2)) {
    id = &(_argMonitorCfg.at("Left").id);
    source = 0x0f;
  } else if (e.keycode == XKeysymToKeycode(_disp, XK_3)) {
    id = &(_argMonitorCfg.at("Main").id);
    source = 0x11;
  }  else if (e.keycode == XKeysymToKeycode(_disp, XK_4)) {
    id = &(_argMonitorCfg.at("Main").id);
    source = 0x0f;
  } else
    return;

  auto it = std::find_if(begin(_monitors), end(_monitors),
                         [&] (const auto& m) { return m.cfg.id == *id; });
  if (it == _monitors.end())
    return;

  _ddc.setSource(*id, source);
  pollDdc(getTime());
}

/// Utils //////////////////////////////////////////////////////////////////////

void Manager::drawGrid(Monitor* mon, bool active)
{
  XClearWindow(_disp, mon->gridDraw);
  XSetWindowBorder(_disp, mon->gridDraw, (active ? GRID_COLOR : GRID_INACT));

  XGCValues values;
  GC gc = XCreateGC(_disp, mon->gridDraw, 0, &values);
  XSetForeground(_disp, gc, (active ? GRID_COLOR : GRID_INACT));
  XSetLineAttributes(_disp, gc, GRID_THICK, LineSolid, CapButt, JoinBevel);

  for (unsigned i = 0; i < mon->gridX - 1; ++i) {
    int x = ((i+1) * (mon->r.w / mon->gridX));
    XDrawLine(_disp, mon->gridDraw, gc, x, 0, x, mon->r.h);
  }

  for (unsigned i = 0; i < mon->gridY - 1; ++i) {
    int y = ((i+1) * (mon->r.h / mon->gridY));
    XDrawLine(_disp, mon->gridDraw, gc, 0, y, mon->r.w, y);
  }
}

Window Manager::getNextWindowInDir(DIR dir, Window w)
{
  std::vector<std::pair<Point, Window>> windows;
  for (const auto& m : _clients) {
    if (m.first != w && !m.second.ign) {
      XWindowAttributes a;
      XGetWindowAttributes(_disp, m.first, &a);
      Point c = m.second.absOrigin + Rect(a.x, a.y, a.width, a.height).getCenter();
      windows.emplace_back(c, m.first);
    }
  }

  XWindowAttributes attr;
  XGetWindowAttributes(_disp, w, &attr);
  Point c = _roots.at(GetWinRoot(_disp, w)).absOrigin + Rect(attr.x, attr.y, attr.width, attr.height).getCenter();

  auto closest = getNextPointInDir(dir, c, windows);
  return closest ? closest : w;
}

void Monitor::setVisible(std::optional<bool> v)
{
  if (this->visible != v)
    LOG(INFO) << "updated monitor visibility name=(" << this->cfg.name << ")"
              << " visible=(" << (v.has_value() ? (v.value() ? "true" : "false") : "unknown") << ")";
  this->visible = v;
}

void Manager::pollDdc(int64_t now)
{
  if (_ddc.poll()) {
    _lastDdcPoll = now;
    for (auto& mon : _monitors) {
      auto source = _ddc.getSource(mon.cfg.id);
      if (source.has_value())
        mon.setVisible(source.value() == mon.cfg.visibleInput);
      else
        mon.setVisible(std::nullopt);
    }
  } else {
    _lastDdcPoll = now - int64_t(DDC_POLL_INTERVAL * 0.9); // Retry soon
    for (auto& mon : _monitors)
      mon.setVisible(std::nullopt);
  }
}
