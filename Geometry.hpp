#pragma once

#include <cfloat>
#include <cmath>
#include <climits>
#include <stdexcept>
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
  int x, y;

  Point()
    : Point(0, 0) {}
  Point(int x, int y)
    : x(x), y(y) {}

  double getDist(const Point& o) const;
  int    getDist(const Point& o, DIR dir) const;
};

struct Rect
{
  Point o;
  int w, h;

  Rect()
    : Rect(0, 0, 0, 0) {}
  Rect(int x, int y, int w, int h)
    : o(x, y), w(w), h(h) {}

  bool contains(const Point& a) const;
  Point getCenter() const;
};

/// Point //////////////////////////////////////////////////////////////////////

inline double Point::getDist(const Point& o) const
{
  // C^2 = A^2 + B^2
  double w = o.x - x;
  double h = o.y - y;
  return sqrt((w*w) + (h*h));
}

inline int Point::getDist(const Point& o, DIR dir) const
{
  int dist;
  switch (dir) {
    case DIR::Up:
      dist = y - o.y;
      break;
    case DIR::Down:
      dist = o.y - y;
      break;
    case DIR::Left:
      dist = x - o.x;
      break;
    case DIR::Right:
      dist = o.x - x;
      break;
    default:
      throw std::runtime_error("invalid DIR enum");
  };
  if (dist < 0)
    dist = INT_MAX;
  return dist;
}

/// Rect ///////////////////////////////////////////////////////////////////////

inline bool Rect::contains(const Point& a) const
{
  return (a.x >= o.x) &&
         (a.x <= o.x + w) &&
         (a.y >= o.y) &&
         (a.y <= o.y + h);

};

inline Point Rect::getCenter() const
{
  Point c(o);
  c.x += (w/2);
  c.y += (h/2);
  return c;
}

/// Misc ///////////////////////////////////////////////////////////////////////

template<typename T>
inline T closestRectFromPoint(const Point& p, const std::vector<std::pair<Rect, T>>& rects)
{
  const T* closest = nullptr;
  double minDist = DBL_MAX;
  for (const auto& r : rects) {
    Point o = r.first.getCenter();
    int vertDist = std::min(p.getDist(o, DIR::Up),   p.getDist(o, DIR::Down));
    int horzDist = std::min(p.getDist(o, DIR::Left), p.getDist(o, DIR::Right));
    auto dist = sqrt((vertDist*vertDist) + (horzDist*horzDist));
    if (dist < minDist) {
      minDist = dist;
      closest = &(r.second);
    }
  }
  return closest ? *closest : T();
}
