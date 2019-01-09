#ifndef CONE_CLASS_HPP_
#define CONE_CLASS_HPP_

#include <tuple>
#include <vector>

#include "include/aabb.hpp"
#include "include/vemath.hpp"

namespace object {
struct Cone {
   // defaults
   Cone(Cone &&) = default;
   Cone(const Cone &) = default;
   Cone &operator=(Cone &&) = default;
   Cone &operator=(const Cone &) = default;
   ~Cone() = default;

   // constructors
   Cone(const vm::Vec3<float> p0, const vm::Vec3<float> p1, const float r0,
        const float r1, const long long fiber_idx, const long long cone_idx)
       : p0(p0), p1(p1), r0(r0), r1(r1), r(r0 > r1 ? r0 : r1),
         fiber_idx(fiber_idx), cone_idx(cone_idx){};

   aabb::AABB<float, 3> aabb() const;

   // colliding solving functions
   bool CollideWith(const Cone cone) const;
   float MinDistanceCones(const Cone cone) const;
   vm::Vec3<float> MinDistanceVectorCones(const Cone cone) const;
   vm::Vec3<float> PushConesApart(const Cone cone) const;

   vm::Vec3<float> p0, p1;
   float r0, r1, r; // TODO: check if r0, r1 are needed
   size_t fiber_idx, cone_idx;
};
} // namespace object

#endif // CONE_CLASS_HPP_