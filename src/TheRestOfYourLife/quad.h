#ifndef QUAD_H
#define QUAD_H
//==============================================================================================
// To the extent possible under law, the author(s) have dedicated all copyright and related and
// neighboring rights to this software to the public domain worldwide. This software is
// distributed without any warranty.
//
// You should have received a copy (see file COPYING.txt) of the CC0 Public Domain Dedication
// along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
//==============================================================================================

#include "rtweekend.h"

#include "hittable.h"
#include "hittable_list.h"

#include <cmath>

class quad : public hittable {
  public:
    quad(const point3& o, const vec3& aa, const vec3& ab, shared_ptr<material> m)
      : plane_origin(o), axis_A(aa), axis_B(ab), mat(m)
    {
        normal = unit_vector(cross(axis_A, axis_B));
        D = -dot(normal, plane_origin);
        area = cross(axis_A, axis_B).length();

        measure_A = axis_A / dot(axis_A, axis_A);
        measure_B = axis_B / dot(axis_B, axis_B);

        set_bounding_box();
    }

    virtual void set_bounding_box() {
        bbox = aabb(plane_origin, plane_origin + axis_A + axis_B).pad();
    }

    aabb bounding_box() const override { return bbox; }

    virtual bool hit_ab(double a, double b, hit_record& rec) const {
        // Given the hit point in plane coordinates, return false if it is outside the
        // primitive, otherwise set the hit record UV coordinates and return true.

        if ((a < 0) || (1 < a) || (b < 0) || (1 < b))
            return false;

        rec.u = a;
        rec.v = b;
        return true;
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        auto denom = dot(normal, r.direction());

        // No hit if the ray is parallel to the plane.
        if (fabs(denom) < 1e-8)
            return false;

        // Return false if the hit point parameter t is outside the ray interval.
        auto t = (-D - dot(normal, r.origin())) / denom;
        if (!ray_t.contains(t))
            return false;

        // Determine the hit point lies within the planar shape using its plane coordinates.
        auto intersection = r.at(t);
        vec3 planar_hitpt_vector = intersection - plane_origin;
        auto a = dot(planar_hitpt_vector, measure_A) ;
        auto b = dot(planar_hitpt_vector, measure_B);

        if (!hit_ab(a, b, rec))
            return false;

        // Ray hits the 2D shape; set the rest of the hit record and return true.
        rec.t = t;
        rec.p = intersection;
        rec.mat = mat;
        rec.set_face_normal(r, normal);

        return true;
    }

    double pdf_value(const point3& origin, const vec3& v) const override {
        hit_record rec;
        if (!this->hit(ray(origin, v), interval(0.001, infinity), rec))
            return 0;

        auto distance_squared = rec.t * rec.t * v.length_squared();
        auto cosine = fabs(dot(v, rec.normal) / v.length());

        return distance_squared / (cosine * area);
    }

    vec3 random(const point3& origin) const override {
        auto p = plane_origin + (random_double() * axis_A) + (random_double() * axis_B);
        return p - origin;
    }

  protected:
    point3 plane_origin;
    vec3 axis_A, axis_B;
    shared_ptr<material> mat;
    vec3 normal;
    double D;
    double area;
    vec3 measure_A, measure_B;
    aabb bbox;
};


inline shared_ptr<hittable_list> box(const point3& a, const point3& b, shared_ptr<material> mat)
{
    // Returns the 3D box (six sides) that contains the two opposite vertices a & b.

    auto sides = make_shared<hittable_list>();

    // Construct the two opposite vertices with the minimum and maximum coordinates.
    auto min = point3(fmin(a.x(), b.x()), fmin(a.y(), b.y()), fmin(a.z(), b.z()));
    auto max = point3(fmax(a.x(), b.x()), fmax(a.y(), b.y()), fmax(a.z(), b.z()));

    auto dx = vec3(max.x() - min.x(), 0, 0);
    auto dy = vec3(0, max.y() - min.y(), 0);
    auto dz = vec3(0, 0, max.z() - min.z());

    sides->add(make_shared<quad>(point3(min.x(), min.y(), max.z()),  dx,  dy, mat)); // front
    sides->add(make_shared<quad>(point3(max.x(), min.y(), max.z()), -dz,  dy, mat)); // right
    sides->add(make_shared<quad>(point3(max.x(), min.y(), min.z()), -dx,  dy, mat)); // back
    sides->add(make_shared<quad>(point3(min.x(), min.y(), min.z()),  dz,  dy, mat)); // left
    sides->add(make_shared<quad>(point3(min.x(), max.y(), max.z()),  dx, -dz, mat)); // top
    sides->add(make_shared<quad>(point3(min.x(), min.y(), min.z()),  dx,  dz, mat)); // bottom

    return sides;
}


class tri : public quad {
  public:
    tri(const point3& o, const vec3& aa, const vec3& ab, shared_ptr<material> m)
      : quad(o, aa, ab, m)
    {}

    virtual bool hit_ab(double a, double b, hit_record& rec) const override {
        if ((a < 0) || (b < 0) || (a + b > 1))
            return false;

        rec.u = a;
        rec.v = b;
        return true;
    }
};


class ellipse : public quad {
  public:
    ellipse(
        const point3& center, const vec3& side_A, const vec3& side_B, shared_ptr<material> m
    ) : quad(center, side_A, side_B, m)
    {}

    virtual void set_bounding_box() override {
        bbox = aabb(plane_origin - axis_A - axis_B, plane_origin + axis_A + axis_B).pad();
    }

    virtual bool hit_ab(double a, double b, hit_record& rec) const override {
        if ((a*a + b*b) > 1)
            return false;

        rec.u = a/2 + 0.5;
        rec.v = b/2 + 0.5;
        return true;
    }
};


class annulus : public quad {
  public:
    annulus(
        const point3& center, const vec3& side_A, const vec3& side_B, double _inner,
        shared_ptr<material> m)
      : quad(center, side_A, side_B, m), inner(_inner)
    {}

    virtual void set_bounding_box() override {
        bbox = aabb(plane_origin - axis_A - axis_B, plane_origin + axis_A + axis_B).pad();
    }

    virtual bool hit_ab(double a, double b, hit_record& rec) const override {
        auto center_dist = sqrt(a*a + b*b);
        if ((center_dist < inner) || (center_dist > 1))
            return false;

        rec.u = a/2 + 0.5;
        rec.v = b/2 + 0.5;
        return true;
    }

  private:
    double inner;
};


#endif
