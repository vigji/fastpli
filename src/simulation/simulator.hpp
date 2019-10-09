#ifndef SIMULATION_SIMULATOR_HPP_
#define SIMULATION_SIMULATOR_HPP_

#include <functional>
#include <memory>
#include <vector>

#include "include/omp.hpp"
#include "include/vemath.hpp"
#include "my_mpi.hpp"
#include "objects/np_array_container.hpp"
#include "setup.hpp"

class PliSimulator {
 public:
   // PliSimulator() = default;
   PliSimulator() { set_omp_num_threads(1); }
   ~PliSimulator() = default;

   void SetMPIComm(const MPI_Comm comm);
   void SetSetup(const setup::Setup setup);

   vm::Vec3<long long> GetImageDim() {
      return vm::Vec3<long long>(
          dim_.local.x(), dim_.local.y(),
          static_cast<long long>(setup_->filter_rotations.size()));
   };

   std::vector<double>
   RunSimulation(const vm::Vec3<long long> &dim,
                 object::container::NpArray<int> label_field,
                 object::container::NpArray<float> vector_field,
                 setup::PhyProps properties, const setup::Tilting tilt);

   int set_omp_num_threads(int num);

 private:
#ifndef NDEBUG
   const bool debug_ = true;
#else
   const bool debug_ = false;
#endif

   setup::Dimensions dim_{};
   std::unique_ptr<setup::Setup> setup_;
   setup::PhyProps properties_;
   object::container::NpArray<int> label_field_;
   object::container::NpArray<float> vector_field_;

   std::vector<vm::Vec4<double>> stored_s_vec_;

   std::unique_ptr<MyMPI> mpi_;

   void CalculateDimensions(const vm::Vec3<long long> &global_dim);
   void CheckDimension();

   int GetLabel(const long long x, const long long y, long long z) const;
   int GetLabel(const vm::Vec3<long long> p) const {
      return GetLabel(p.x(), p.y(), p.z());
   };
   int GetLabel(const double x, const double y, const double z) const;
   int GetLabel(const vm::Vec3<double> p) const {
      return GetLabel(p.x(), p.y(), p.z());
   };

   vm::Vec3<double> GetVec(const long long x, const long long y,
                           long long z) const;
   vm::Vec3<double> GetVec(const vm::Vec3<long long> p) const {
      return GetVec(p.x(), p.y(), p.z());
   };
   vm::Vec3<double> GetVec(const double x, const double y, const double z,
                           const bool interpolate) const;
   vm::Vec3<double> GetVec(const vm::Vec3<double> p,
                           const bool interpolate) const {
      return GetVec(p.x(), p.y(), p.z(), interpolate);
   };
   vm::Vec3<double> InterpolateVec(const double x, const double y,
                                   double z) const;

   vm::Vec3<double> LightDirectionUnitVector(const setup::Tilting tilt) const;
   vm::Vec3<int>
   LightDirectionComponent(const vm::Vec3<double> &direction_vec) const;
   std::vector<setup::Coordinates>
   CalcStartingLightPositions(const setup::Tilting &tilt);
   std::vector<setup::Coordinates>
   CalcStartingLightPositionsTilted(const setup::Tilting &tilt);
   std::vector<setup::Coordinates>
   CalcStartingLightPositionsUntilted(const setup::Tilting &tilt);

   bool CheckMPIHalo(const vm::Vec3<double> &local_pos,
                     const vm::Vec2<long long> &ccd_pos,
                     const vm::Vec3<int> &shift_direct,
                     const std::vector<vm::Vec4<double>> &s_vec);

   void Abort(const int num) const;
};

#endif // SIMULATION_SIMULATOR_HPP_
