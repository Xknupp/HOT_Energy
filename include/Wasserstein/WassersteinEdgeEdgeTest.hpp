// WassersteinEdgeEdgeTest.hpp

#ifndef WASSERSTEINEDGEEDGETEST_HPP
#define WASSERSTEINEDGEEDGETEST_HPP

#include "WassersteinKernel.hpp"

class WassersteinEdgeEdgeTest
{

public:

  // return the coordinates of the segment, as a nicely formatted string
  static
  std::string pretty_print( const Segment &e );

  // return the point i/(n-1) from e.source to e.target
  static
  Point frac(const Segment &e, int i, int n)
  {
    return e.source() + (i/ (double)(n-1)) * e.to_vector();
  }

  // compute W2 for two perpendicular segments, several different ways, to verify
  static
  double w2_perp(const Segment &e0, const Segment &e1);

  // compute W_p for two segments in general position, using brute force integration,
  // considering all permutations of the order of points
  // (more than 7 points might take a long time)
  // p is the order of W, typically p=1,2
  static
  double w_p(int p, const Segment &e0, const Segment &e1);

  // test the above for a bunch of different segments
  static
  void test();

};

// e.g.
/*
int main(int argc, char **argv) {
  
  // Test Wasserstein distance between two segments
  if (1)
  {
    WassersteinEdgeEdgeTest::test();
    return 0;
  }
}
*/

#endif // WASSERSTEINEDGEEDGETEST_HPP
