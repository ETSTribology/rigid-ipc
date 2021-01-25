#pragma once

#include <Eigen/Core>

#include <ipc/collision_constraint.hpp>

#include <opt/collision_constraint.hpp>

#include <autodiff/autodiff_types.hpp>
#include <barrier/barrier.hpp>
#include <ccd/ccd.hpp>
#include <ipc/spatial_hash/hash_grid.hpp>
#include <utils/eigen_ext.hpp>

namespace ccd {
namespace opt {

    class DistanceBarrierConstraint : public CollisionConstraint {
    public:
        DistanceBarrierConstraint(
            const std::string& name = "distance_barrier_constraint");

        nlohmann::json settings() const override;
        void settings(const nlohmann::json& json) override;

        virtual void initialize() override;

        double barrier_activation_distance() const
        {
            return m_barrier_activation_distance;
        }
        void barrier_activation_distance(const double dhat)
        {
            m_barrier_activation_distance = dhat;
        }

        bool has_active_collisions(
            const physics::RigidBodyAssembler& bodies,
            const physics::Poses<double>& poses_t0,
            const physics::Poses<double>& poses_t1) const;

        double compute_earliest_toi(
            const physics::RigidBodyAssembler& bodies,
            const physics::Poses<double>& poses_t0,
            const physics::Poses<double>& poses_t1) const;

        void compute_constraints(
            const physics::RigidBodyAssembler& bodies,
            const physics::Poses<double>& poses,
            Eigen::VectorXd& barriers);

        void construct_constraint_set(
            const physics::RigidBodyAssembler& bodies,
            const physics::Poses<double>& poses,
            ipc::Constraints& constraint_set) const;

        template <typename T>
        T distance_barrier(const T& distance, const double dhat) const;

        template <typename T> T distance_barrier(const T& distance) const
        {
            return distance_barrier(distance, m_barrier_activation_distance);
        }

        // TODO: Add distance_barrier_gradient if necessary

        double distance_barrier_hessian(double distance) const
        {
            return barrier_hessian(
                distance, m_barrier_activation_distance, barrier_type);
        }

        double compute_minimum_distance(
            const physics::RigidBodyAssembler& bodies,
            const physics::Poses<double>& poses) const;

        // Settings
        // ----------
        /// @brief Initial d̂ to use in the barrier function.
        double initial_barrier_activation_distance;

        /// @brief Choice of barrier function (see barrier/barrier.hpp).
        BarrierType barrier_type;

        double minimum_separation_distance;

    protected:
        bool has_active_collisions_narrow_phase(
            const physics::RigidBodyAssembler& bodies,
            const physics::Poses<double>& poses_t0,
            const physics::Poses<double>& poses_t1,
            const ipc::Candidates& candidates) const;

        double compute_earliest_toi_narrow_phase(
            const physics::RigidBodyAssembler& bodies,
            const physics::Poses<double>& poses_t0,
            const physics::Poses<double>& poses_t1,
            const ipc::Candidates& candidates) const;

        /// @brief Max distance, d̂, at which the barrier forces are activate.
        double m_barrier_activation_distance;
    };

} // namespace opt
} // namespace ccd

#include "distance_barrier_constraint.tpp"
