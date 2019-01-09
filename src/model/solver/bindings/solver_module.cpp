#include <iostream>
#include <tuple>

#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "../world.hpp"
#include "objects/fiber.hpp"

namespace py = pybind11;

PYBIND11_MODULE(_solver_cpp, m) {
   m.doc() = "Fiber Volume Colliding Solver";

   py::class_<World>(m, "_SolverCPP")
       .def(py::init())
       //  .def_property("fiber_bundles", &World::get_fibers,
       //  &World::set_fibers)
       .def("get_fiber_bundles", &World::get_fibers)
       .def("set_fiber_bundles", &World::set_fibers)
       .def("set_omp_num_threads", &World::set_omp_num_threads)
       .def("set_parameters",
            [](World &self, float drag, float obj_min_radius,
               float obj_mean_length) {
               World::WorldParameter wp;
               wp.drag = drag;
               wp.obj_min_radius = obj_min_radius;
               wp.obj_mean_length = obj_mean_length;
               self.set_parameter(wp);
            },
            py::arg("drag") = 0, py::arg("obj_min_radius") = 10,
            py::arg("obj_mean_length") = 10)
       .def("get_parameters",
            [](World &self) {
               auto p = self.get_parameter();
               return std::make_tuple(p.drag, p.obj_min_radius,
                                      p.obj_mean_length);
            })
       .def(
           "set_col_voi",
           [](World &self, std::array<float, 3> min, std::array<float, 3> max) {
              self.set_colliding_voi(aabb::AABB<float, 3>(min, max));
           })
       .def("step", (bool (World::*)(void)) & World::Step)
       .def_property_readonly("num_obj", &World::NumObj)
       .def_property_readonly("num_col_obj", &World::NumColObj)
       .def("draw_scene", &World::DrawScene, py::arg("rot_x") = 30,
            py::arg("rot_y") = 30, py::arg("rot_z") = 0);
}