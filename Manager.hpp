#pragma once

#include <X11/Xlib.h>

#include <limits.h>
#include <map>
#include <vector>

enum class DIR
{
  Up,
  Down,
  Left,
  Right,
  Last
};

struct Drag
{
  Window w;

  int xR, yR;
  int x, y;
  int width, height;

  int btn;
  DIR dir;
};

struct Client
{
  Window client;
  Window root;
};

struct Monitor
{
  std::string name;
  int x, y;
  int w, h;

  bool contains(int ox, int oy) const;
};

struct ScreenInfo
{
  int num;
  std::vector<Monitor> monitors;
};

class Manager
{
  public:

    Manager(const std::string& display,
            const std::vector<int>& screens);
    ~Manager();

    bool init();
    void run();

    void onReq_Map(const XMapRequestEvent& e);
    void onReq_Configure(const XConfigureRequestEvent& e);
    void onNot_Unmap(const XUnmapEvent& e);
    void onNot_Motion(const XButtonEvent& e);
    void onKeyPress(const XKeyEvent& e);
    void onBtnPress(const XButtonEvent& e);

    void addClient(Window w);
    void delClient(Client& c);

    Window getRoot(Window w);
    Window getNextWindowInDir(DIR dir, Window w);

  private:
    const std::string& _argDisp;
    const std::vector<int>& _argScreens;

    Display* _disp;
    int _numScreens;
    std::map<Window /*client*/, Client> _clients;
    std::map<Window /*root*/, ScreenInfo> _screens;
    Drag _drag;
};

static inline std::string ToString(const XEvent& e) {
  static const char* const X_EVENT_TYPE_NAMES[] = {
      "",
      "",
      "KeyPress",
      "KeyRelease",
      "ButtonPress",
      "ButtonRelease",
      "MotionNotify",
      "EnterNotify",
      "LeaveNotify",
      "FocusIn",
      "FocusOut",
      "KeymapNotify",
      "Expose",
      "GraphicsExpose",
      "NoExpose",
      "VisibilityNotify",
      "CreateNotify",
      "DestroyNotify",
      "UnmapNotify",
      "MapNotify",
      "MapRequest",
      "ReparentNotify",
      "ConfigureNotify",
      "ConfigureRequest",
      "GravityNotify",
      "ResizeRequest",
      "CirculateNotify",
      "CirculateRequest",
      "PropertyNotify",
      "SelectionClear",
      "SelectionRequest",
      "SelectionNotify",
      "ColormapNotify",
      "ClientMessage",
      "MappingNotify",
      "GeneralEvent",
  };

  if (e.type < 2 || e.type >= LASTEvent) {
    return "Unknown";
  }

  return X_EVENT_TYPE_NAMES[e.type];
}

struct Point
{
  int x;
  int y;

  Point() : Point(0, 0) {}
  Point(int x, int y) : x(x), y(y) {}
};

static inline Point getCenter(int x, int y, int w, int h)
{
  Point p;
  p.x = x + (w/2);
  p.y = y + (h/2);
  return p;
}

static inline int getDist(Point a, Point b, DIR dir)
{
  int dist;
  switch (dir) {
    case DIR::Up:
      dist = a.y - b.y;
      break;
    case DIR::Down:
      dist = b.y - a.y;
      break;
    case DIR::Left:
      dist = a.x - b.x;
      break;
    case DIR::Right:
      dist = b.x - a.x;
      break;
    default:
      throw std::runtime_error("invalid DIR enum");
  };
  if (dist < 0) dist = INT_MAX;

  return dist;
}
