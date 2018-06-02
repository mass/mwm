#pragma once

#include <X11/Xlib.h>

#include <map>
#include <vector>

class Manager
{
  public:

    Manager();
    ~Manager();

    bool init();
    void run();

    void onReq_Map(const XMapRequestEvent& e);
    void onReq_Configure(const XConfigureRequestEvent& e);
    void onNot_Unmap(const XUnmapEvent& e);

    void frame(Window w);
    void unframe(Window w);
    Window getRoot(Window w);

  private:

    Display* _disp;
    int _numScreens;
    std::map<Window, Window> _frames;
};
