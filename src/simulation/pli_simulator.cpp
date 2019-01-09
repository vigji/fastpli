#include "pli_simulator.hpp"

#include <cassert>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

#include "include/vemath.hpp"
#include "objects/vector_container.hpp"

// PliSimulator::PliSimulator() {}
// PliSimulator::~PliSimulator() {}

void PliSimulator::SetPliSetup(const PliSetup pli_setup) {

   if (pli_setup.light_intensity < 0)
      throw std::invalid_argument("light intensity < 0: " +
                                  std::to_string(pli_setup.light_intensity));

   if (pli_setup.resolution <= 0)
      throw std::invalid_argument("resolution <= 0: " +
                                  std::to_string(pli_setup.resolution));

   if (pli_setup.wavelength <= 0)
      throw std::invalid_argument("wavelength <= 0: " +
                                  std::to_string(pli_setup.wavelength));

   if (pli_setup.filter_rotations.empty())
      throw std::invalid_argument("filter_rotations is empty: []");

   pli_setup_ = pli_setup;
}

void PliSimulator::SetTissue(data::VectorContainer<int> label_field,
                             data::VectorContainer<float> vector_field,
                             const std::array<int, 3> &dim,
                             const std::vector<TissueProperty> &properties,
                             const double pixel_size) {
   auto dim_vec = vm::Vec3<int>(dim[0], dim[1], dim[2]);
   SetTissue(label_field, vector_field, dim_vec, properties, pixel_size);
}

void PliSimulator::SetTissue(data::VectorContainer<int> label_field,
                             data::VectorContainer<float> vector_field,
                             const vm::Vec3<int> &dim,
                             const std::vector<TissueProperty> &properties,
                             const double pixel_size)

{
   if (pixel_size <= 0)
      throw std::invalid_argument("pixel_size <= 0: " +
                                  std::to_string(pixel_size));

   if (dim.x() <= 0 || dim.y() <= 0 || dim.z() <= 0)
      throw std::invalid_argument("dim[any] <= 0: [" + std::to_string(dim.x()) +
                                  "," + std::to_string(dim.y()) + "," +
                                  std::to_string(dim.z()) + "]");

   auto udim = vm::cast<size_t>(dim);
   auto size = udim.x() * udim.y() * udim.z();

   if (size != label_field.size())
      throw std::invalid_argument(
          "dimension and label_field aren't consistend: " +
          std::to_string(size) + " != " + std::to_string(label_field.size()));

   if (size * 3 != vector_field.size())
      throw std::invalid_argument(
          "dimension and vector_field aren't consistend: " +
          std::to_string(size) +
          "*3 != " + std::to_string(vector_field.size()));

   auto max_label = static_cast<size_t>(
       *max_element(label_field.begin(), label_field.end()));
   if (max_label >= properties.size())
      throw std::invalid_argument(
          "max(label) exits propertie.size: " + std::to_string(max_label) +
          ">=" + std::to_string(properties.size()));

   label_field_ = label_field;
   vector_field_ = vector_field;
   dim_ = vm::cast<size_t>(dim);
   properties_ = properties;
   pixel_size_ = pixel_size;
}

std::vector<float> PliSimulator::RunSimulation(const double theta,
                                               const double phi,
                                               const double step_size,
                                               const bool do_nn
                                               // , const bool flip_beam
) {

   if (std::abs(theta) >= M_PI_2)
      throw std::invalid_argument("illegal light path");

   if (step_size <= 0)
      throw std::invalid_argument("step_size <= 0: " +
                                  std::to_string(step_size));

   const auto n_rho = pli_setup_.filter_rotations.size();
   const double lambda = pli_setup_.wavelength * 1e-9;
   const double thickness = pixel_size_ * 1e-6;

   const double pol_x = 1; // TODO: via pli_setup
   const double pol_y = 1;

   vm::Mat4x4<double> polarizer_x(0);
   vm::Mat4x4<double> polarizer_y(0);
   vm::Mat4x4<double> m_lambda_4 = RetarderMatrix(M_PI_2, -M_PI_2);

   // FIXME: WRONG!! for p != 1
   polarizer_x(0, 0) = pol_x * pol_x;
   polarizer_x(0, 1) = pol_x * pol_x;
   polarizer_x(1, 0) = pol_x * pol_x;
   polarizer_x(1, 1) = pol_x * pol_x;

   polarizer_y(0, 0) = pol_y * pol_y;
   polarizer_y(0, 1) = -pol_y * pol_y;
   polarizer_y(1, 0) = -pol_y * pol_y;
   polarizer_y(1, 1) = pol_y * pol_y;

   polarizer_x *= 0.5;
   polarizer_y *= 0.5;

   vm::Vec4<double> signal_0 = {{pli_setup_.light_intensity, 0, 0, 0}};
   signal_0 = vm::dot(m_lambda_4, vm::dot(polarizer_x, signal_0));
   const std::vector<vm::Vec4<double>> s_vec_0(n_rho, signal_0);

   const vm::Mat3x3<double> rotation =
       vm::rot_pi_cases::Mat3RotZYZ(phi, theta, -phi);
   const vm::Vec3<double> step = TiltDirection(theta, phi) * step_size;
   const auto TransformSensorPosToStart =
       GetSensorToStartTransformation(theta, phi);

   std::vector<float> intensity_signal(
       static_cast<size_t>(dim_.x()) * dim_.y() * n_rho, 0);

   // calculate light for all (x,y) positions of the ccd-sensor
   for (unsigned int ccd_x = 0; ccd_x < dim_.x(); ++ccd_x) {
      for (unsigned int ccd_y = 0; ccd_y < dim_.y(); ++ccd_y) {
         size_t ccd_idx = ccd_x * dim_.y() + ccd_y;
         assert(ccd_idx < intensity_signal.size());

         auto s_vec = s_vec_0;
         auto pos = TransformSensorPosToStart(ccd_x, ccd_y);
         for (; pos > -0.5 && pos < vm::cast<double>(dim_) - 0.5; pos += step) {
            auto label = GetLabel(pos);

            // calculate physical parameters
            double dn = properties_[label].dn;
            double mu = properties_[label].mu * 1e3;
            const auto attenuation = pow(exp(-0.5 * mu * thickness), 2);

            if (dn == 0) {
               if (mu == 0)
                  continue;

               for (auto rho = 0u; rho < n_rho; rho++)
                  s_vec[rho] *= attenuation;
               continue;
            }

            auto vec = GetVec(pos, do_nn);
            const auto d_rel = dn * 4.0 * thickness / lambda;

            if (theta != 0)
               vec = vm::dot(rotation, vec);

            // calculate spherical coordinates
            const auto alpha = asin(vec.z());
            const auto ret =
                M_PI_2 * d_rel * pow(cos(alpha), 2.0); // M_PI_2 = pi/2
            const auto sin_r = sin(ret);
            const auto cos_r = cos(ret);

            const auto phii = atan2(vec.y(), vec.x());

            for (auto rho = 0u; rho < n_rho; rho++) {
               const auto beta = 2 * (pli_setup_.filter_rotations[rho] - phii);
               const auto sin_b = sin(-beta);
               const auto cos_b = cos(-beta);

               auto a1 = s_vec[rho][1] * cos_b - s_vec[rho][2] * sin_b;
               auto c1 = s_vec[rho][1] * sin_b + s_vec[rho][2] * cos_b;
               auto b1 = c1 * cos_r + s_vec[rho][3] * sin_r;

               // is equivalent to (R*M*R*S)*att
               s_vec[rho] = {{s_vec[rho][0], a1 * cos_b + b1 * sin_b,
                              -a1 * sin_b + b1 * cos_b,
                              -c1 * sin_r + s_vec[rho][3] * cos_r}};
               s_vec[rho] *= attenuation;
            }
         }

         // save signal only if light beam did not leave xy border
         if (pos.x() >= -0.5 && pos.y() >= -0.5 &&
             pos.x() < static_cast<double>(dim_.x()) - 0.5 &&
             pos.y() < static_cast<double>(dim_.y()) - 0.5) {
            for (auto rho = 0u; rho < n_rho; rho++) {
               s_vec[rho] = vm::dot(polarizer_y, s_vec[rho]);
               intensity_signal[ccd_idx * n_rho + rho] = s_vec[rho][0];
            }
         }
      }
   }

   return intensity_signal;
}

int PliSimulator::GetLabel(const long long x, const long long y,
                           const long long z) const {
   size_t idx = x * dim_.y() * dim_.z() + y * dim_.z() + z;
   assert(idx < label_field_.size());
   return label_field_[idx];
}

vm::Vec3<double> PliSimulator::GetVec(const long long x, const long long y,
                                      const long long z) const {
   size_t idx = x * dim_.y() * dim_.z() * 3 + y * dim_.z() * 3 + z * 3;
   assert(idx < vector_field_.size());
   return vm::Vec3<double>(vector_field_[idx], vector_field_[idx + 1],
                           vector_field_[idx + 2]);
}
vm::Vec3<double> PliSimulator::GetVec(const double x, const double y,
                                      const double z, const bool do_nn) const {
   return InterpolateVec(x, y, z, do_nn);
}

// TODO: template do_nn
vm::Vec3<double> PliSimulator::InterpolateVec(const double x, const double y,
                                              const double z,
                                              const bool do_nn) const {

   // NN interpolation
   if (do_nn)
      return GetVec(static_cast<long long>(std::round(x)),
                    static_cast<long long>(std::round(y)),
                    static_cast<long long>(std::round(z)));

   // Trilinear interpolation
   auto x0 = static_cast<long long>(std::floor(x));
   auto y0 = static_cast<long long>(std::floor(y));
   auto z0 = static_cast<long long>(std::floor(z));

   auto x1 = static_cast<long long>(std::ceil(x));
   auto y1 = static_cast<long long>(std::ceil(y));
   auto z1 = static_cast<long long>(std::ceil(z));

   auto dim = vm::cast<long long>(dim_);

   if (x0 < 0)
      x0 = 0;
   if (y0 < 0)
      y0 = 0;
   if (z0 < 0)
      z0 = 0;

   if (x1 >= dim.x())
      x1 = dim.x() - 1;
   if (y1 >= dim.y())
      y1 = dim.y() - 1;
   if (z1 >= dim.z())
      z1 = dim.z() - 1;

   if (x0 == x1 && y0 == y1 && z0 == z1)
      return GetVec(x0, y0, z0);

   auto xd = (x - x0) / (x1 - x0);
   auto yd = (y - y0) / (y1 - y0);
   auto zd = (z - z0) / (z1 - z0);

   if (x0 == x1)
      xd = 0;
   if (y0 == y1)
      yd = 0;
   if (z0 == z1)
      zd = 0;

   auto c000 = GetVec(x0, y0, z0);
   auto c100 = GetVec(x1, y0, z0);
   auto c010 = GetVec(x0, y1, z0);
   auto c110 = GetVec(x1, y1, z0);
   auto c001 = GetVec(x0, y0, z1);
   auto c101 = GetVec(x1, y0, z1);
   auto c011 = GetVec(x0, y1, z1);
   auto c111 = GetVec(x1, y1, z1);

   auto c00 = c000 * (1 - xd) + c100 * xd;
   auto c01 = c001 * (1 - xd) + c101 * xd;
   auto c10 = c010 * (1 - xd) + c110 * xd;
   auto c11 = c011 * (1 - xd) + c111 * xd;

   auto c0 = c00 * (1 - yd) + c10 * yd;
   auto c1 = c01 * (1 - yd) + c11 * yd;

   return c0 * (1 - zd) + c1 * zd;
}

vm::Vec3<double> PliSimulator::TiltDirection(const double theta,
                                             const double phi) const {

   // special tilting angles
   if (theta == 0)
      return vm::Vec3<double>(0, 0, 1);
   else if (phi == 0)
      return vm::Vec3<double>(sin(theta), 0.0, cos(theta));
   else if (phi == M_PI_2)
      return vm::Vec3<double>(0.0, sin(theta), cos(theta));
   else if (phi == M_PI)
      return vm::Vec3<double>(-sin(theta), 0.0, cos(theta));
   else if (phi == 3 * M_PI_2)
      return vm::Vec3<double>(0.0, -sin(theta), cos(theta));

   return vm::Vec3<double>(cos(phi) * sin(theta), sin(phi) * sin(theta),
                           cos(theta));
}

std::function<vm::Vec3<double>(int, int)>
PliSimulator::GetSensorToStartTransformation(const double theta,
                                             const double phi) const {

   if (pli_setup_.untilt_sensor) {
      const auto dz = dim_.z() - 1;
      // FIXME: const auto dz = dim_.z() - 0.5;

      // special tilting angles
      vm::Vec2<double> shift;
      if (theta == 0)
         shift = vm::Vec2<double>(0.0, 0.0);
      else if (phi == 0)
         shift = vm::Vec2<double>(tan(theta) * dz, 0.0);
      else if (phi == M_PI_2)
         shift = vm::Vec2<double>(0.0, tan(theta) * dz);
      else if (phi == M_PI)
         shift = vm::Vec2<double>(-tan(theta) * dz, 0.0);
      else if (phi == 3 * M_PI_2)
         shift = vm::Vec2<double>(0.0, -tan(theta) * dz);
      else
         shift =
             vm::Vec2<double>(tan(theta) * cos(phi), tan(theta) * sin(phi)) *
             dz;

      return [=](int x, int y) -> vm::Vec3<double> {
         return vm::Vec3<double>(x - shift.x(), y - shift.y(), 0.0);
      };
   } else {
      // TODO: implement back projection
      return std::function<vm::Vec3<double>(int, int)>();
   }
}

vm::Mat4x4<double> PliSimulator::RetarderMatrix(const double beta,
                                                const double ret) const {
   vm::Mat4x4<double> M = {
       {1, 0, 0, 0, 0, pow(cos(beta), 2) + cos(ret) * pow(sin(beta), 2),
        (1 - cos(ret)) * sin(beta) * cos(beta), -sin(ret) * sin(beta), 0,
        (1 - cos(ret)) * sin(beta) * cos(beta),
        pow(sin(beta), 2) + cos(ret) * pow(cos(beta), 2), sin(ret) * cos(beta),
        0, sin(ret) * sin(beta), -sin(ret) * cos(beta), cos(ret)}};
   return M;
}