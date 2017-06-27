
#ifndef _HOT_HPP_
#define _HOT_HPP_

#include <CGAL/Cartesian.h>
#include <CGAL/Delaunay_triangulation_2.h>
#include <CGAL/Root_of_traits.h>
#include <CGAL/number_utils.h>

#include <boost/variant/variant.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <list>
#include <vector>

#include <polynomial.hpp>

constexpr const int dims = 2;

using K = CGAL::Cartesian<double>;
// real type underlying K
using K_real = K::RT;

using DT = CGAL::Delaunay_triangulation_2<K>;
using Face = DT::Face;
using Point = DT::Point;
using Vertex = DT::Vertex;

using Triangle = CGAL::Triangle_2<K>;
using Line = CGAL::Line_2<K>;
using Segment = CGAL::Segment_2<K>;
using Vector = CGAL::Vector_2<K>;
using Direction = CGAL::Direction_2<K>;

constexpr const int tri_verts = 3;
constexpr const int tri_edges = 3;

struct finite_diffs {
  DT::Vertex_handle vtx;
  K_real center;
  K_real dx_plus;
  K_real dx_minus;
  K_real dy_plus;
  K_real dy_minus;
};

void order_points(std::array<Point, tri_verts> &verts);
Triangle face_to_tri(const Face &face);

double wasserstein2_edge_edge(const Segment &e0, const Segment &e1);

std::list<DT::Vertex_handle> internal_vertices(const DT &dt);

/* Computes 1 or 2 triangles which can be used for the
 * piecewise integral of the Wasserstein distance with
 * k */
boost::variant<std::array<Triangle, 2>, std::array<Triangle, 1>>
integral_bounds(const Triangle &face);

/* Computes the circumcenter of the triangle */
Point triangle_circumcenter(const Triangle &face);

template <typename T, int k> class triangle_w_helper;

template <typename T> class triangle_w_helper<T, 2> {
public:
  K_real operator()(T &bounds, const Point &circumcenter) {
    K_real integral = 0;
    for (auto area : bounds) {
      /* Start by computing the lines used for boundaries
       * and the width.
       */
      std::array<Point, tri_verts> verts = {area.vertex(0), area.vertex(1),
                                            area.vertex(2)};
      order_points(verts);

      /* Find the index of the non-vertical point
       * It's guaranteed to be either the
       * leftmost or
       * rightmost point
       */
      const int w_idx = (verts[0][0] != verts[1][0]) ? 0 : 2;
      // Compute the signed width
      K_real width = verts[w_idx][0] - verts[(w_idx + 1) % tri_verts][0];
      /* Compute the bounding lines
       * x = ax t + bx -> t = (x - bx) /
       * ax
       * y = ay t + by = (ay / ax) x +
       * (-ay bx / ax + by)
       *
       * bx = verts[w_idx][0], by = verts[w_idx][1]
       */
      Vector dir_1 =
          Line(verts[w_idx], verts[(w_idx + 1) % tri_verts]).to_vector();
      Vector dir_2 =
          Line(verts[w_idx], verts[(w_idx + 2) % tri_verts]).to_vector();
      K_real slope_1 = dir_1[1] / dir_1[0];
      K_real slope_2 = dir_2[1] / dir_2[0];
      if ((w_idx == 0 && slope_1 < slope_2) ||
          (w_idx == 2 && slope_1 > slope_2)) {
        std::swap(slope_1, slope_2);
      }
      Numerical::Polynomial<K_real, 1, 1> bound_1;
      bound_1.coeff(1) = slope_1;
      bound_1.coeff(0) = -verts[w_idx][0] * slope_1 + verts[w_idx][1];
      Numerical::Polynomial<K_real, 1, 1> bound_2;
      bound_2.coeff(1) = slope_2;
      bound_2.coeff(0) = -verts[w_idx][0] * slope_2 + verts[w_idx][1];

      Numerical::Polynomial<K_real, 1, 2> initial_x_root((Tags::Zero_Tag()));
      initial_x_root.coeff(1, 0) = 1;
      initial_x_root.coeff(0, 0) = -circumcenter[0];

      Numerical::Polynomial<K_real, 1, 2> initial_y_root((Tags::Zero_Tag()));
      initial_y_root.coeff(0, 1) = 1;
      initial_y_root.coeff(0, 0) = -circumcenter[1];

      Numerical::Polynomial<K_real, 2, 2> initial =
          initial_x_root * initial_x_root + initial_y_root * initial_y_root;

      auto y_int = initial.integrate(1);
      Numerical::Polynomial<K_real, 3, 1> upper = y_int.var_sub(1, bound_1);
      Numerical::Polynomial<double, 3, 1> lower = y_int.var_sub(1, bound_2);
      auto y_bounded = upper + -lower;

      auto x_int = y_bounded.integrate(0);

      auto left = x_int.slice(0, verts[w_idx][0]).coeff(0);
      auto right = x_int.slice(0, verts[(w_idx + 1) % tri_verts][0]).coeff(0);
      integral += std::abs(left + -right);
    }
    return integral;
  }
};

/* Computes the k Wasserstein distance of the triangular
 * face to it's circumcenter */
template <int k> K_real triangle_w(const Triangle &face);

template <> K_real triangle_w<2>(const Triangle &tri) {
  Point circumcenter = triangle_circumcenter(tri);
  boost::variant<std::array<Triangle, 2>, std::array<Triangle, 1>> bounds =
      integral_bounds(tri);
  // Set to NaN until implemented
  K_real distance = 0.0 / 0.0;
  if (bounds.which() == 0) {
    // std::array<Triangle, 2>
    auto area = boost::get<std::array<Triangle, 2>>(bounds);
    return triangle_w_helper<std::array<Triangle, 2>, 2>()(area, circumcenter);
  } else {
    // std::array<Triangle, 1>
    auto area = boost::get<std::array<Triangle, 1>>(bounds);
    return triangle_w_helper<std::array<Triangle, 1>, 2>()(area, circumcenter);
  }
}

/* See "HOT: Hodge-Optimized Triangulations" for details on
 * the energy functional */
template <int k> K_real hot_energy(const DT &dt) {
  K_real energy = 0;
  for (auto face_itr = dt.finite_faces_begin();
       face_itr != dt.finite_faces_end(); face_itr++) {
    Triangle tri = face_to_tri(*face_itr);
    K_real area = tri.area();
    K_real wasserstein = triangle_w<k>(tri);
    energy += wasserstein * area;
  }
  return K_real(float(energy));
}

template <int k>
K_real compute_incident_energies(const DT &dt, DT::Vertex_handle vtx) {
  K_real energy = 0.0;
  bool once = false;
  for (DT::Face_circulator face_itr = dt.incident_faces(vtx),
                           start_face = face_itr;
       !once || face_itr != start_face; face_itr++) {
    once = true;
    Triangle tri = face_to_tri(*face_itr);
    K_real area = tri.area();
    K_real wasserstein = triangle_w<k>(tri);
    energy += area * wasserstein;
  }
  return energy;
}

K_real choose_distance_scale(DT &dt, std::vector<finite_diffs> diffs) {
  // TODO: Implement something real here
  return 1.0;
}

template <int k>
std::vector<finite_diffs>
compute_gradient(DT &dt, std::list<DT::Vertex_handle> &internal_verts) {
  constexpr const K_real dx = 0.0000001;
  constexpr const K_real dy = 0.0000001;
  std::vector<finite_diffs> f_diffs(internal_verts.size());
  int idx = 0;
  for (DT::Vertex_handle vtx : internal_verts) {
    DT::Point initial_point = vtx->point();
    finite_diffs &grad = f_diffs[idx];
    grad.vtx = vtx;

    grad.center = compute_incident_energies<k>(dt, vtx);
    dt.move(vtx, Point(initial_point[0] + dx, initial_point[1]));

    grad.dx_plus = compute_incident_energies<k>(dt, vtx);
    dt.move(vtx, Point(initial_point[0] - dx, initial_point[1]));
    grad.dx_minus = compute_incident_energies<k>(dt, vtx);
    dt.move(vtx, Point(initial_point[0], initial_point[1] + dy));

    grad.dy_plus = compute_incident_energies<k>(dt, vtx);
    dt.move(vtx, Point(initial_point[0], initial_point[1] - dy));
    grad.dy_minus = compute_incident_energies<k>(dt, vtx);

    dt.move(vtx, initial_point);
    idx++;
  }
  return f_diffs;
}

template <int k> DT hot_optimize(DT dt, K_real min_delta_energy = 0.1) {
  K_real delta_energy = std::numeric_limits<K_real>::infinity();
  std::list<DT::Vertex_handle> internal_verts = internal_vertices(dt);
  // This mesh is modified to determine the gradient each step
  while (delta_energy >= min_delta_energy) {
    delta_energy = 0.0;
    // Shift each point by dx and dy separately,
    // measuring how much the energy changes to approximate the gradient
    std::vector<finite_diffs> f_diffs = compute_gradient<k>(dt, internal_verts);
    K_real dist_scale = choose_distance_scale(dt, f_diffs);
    for (finite_diffs diff : f_diffs) {
      K_real dx = dist_scale * (diff.dx_plus - diff.dx_minus) / 2.0;
      K_real dy = dist_scale * (diff.dy_plus - diff.dy_minus) / 2.0;
      dt.move(diff.vtx,
              Point(diff.vtx->point()[0] - dx, diff.vtx->point()[1] - dy));
    }
  }
  return dt;
}

#endif // _HOT_HPP_
