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

struct Point
{
  int x;
  int y;

  Point() : Point(0, 0) {}
  Point(int x, int y) : x(x), y(y) {}
};

struct Rect
{
  Point o;
  int w, h;

  Rect() : Rect(0, 0, 0, 0) {}
  Rect(int x, int y, int w, int h)
    : o(x, y), w(w), h(h) {}
  Rect(Window w, Display* d)
  {
    XWindowAttributes attr;
    XGetWindowAttributes(d, w, &attr);
    o.x = attr.x; o.y = attr.y;
    w = attr.width;
    h = attr.height;
  }

  inline bool contains(const Point& a) const {
    return (a.x >= o.x && a.x <= o.x + w) &&
           (a.y >= o.y && a.y <= o.y + h);
  }

  inline Point getCenter()
  {
    Point c(o);
    c.x += (w/2);
    c.y += (h/2);
    return c;
  }
};

struct Drag
{
  Window w;

  int xR, yR;
  int x, y;
  int width, height;

  int btn;
  DIR dirVert;
  DIR dirHorz;
};

struct Client
{
  Window client;
  Window root;

  Rect preMax;

  bool ign;

  inline Rect getRect(Display* disp) {
    return {client, disp};
  }
};

struct Monitor
{
  std::string name;
  Rect r;

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

    void switchFocus(Window w);
    void handleFocusChange(Window w, bool in);

    void addClient(Window w, bool checkIgn);
    void delClient(Client& c);

    Window getRoot(Window w);
    Window getNextWindowInDir(DIR dir, Window w);

    template<typename T>
    T closestRectFromPoint(const Point& p, std::vector<std::pair<Rect, T>>& rects);

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
