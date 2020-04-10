#include "exponential_euler_time_stepper.hpp"

namespace ccd {
namespace time_stepper {

    static void project_orientation(Eigen::Matrix3d& R)
    {
        Eigen::JacobiSVD<Eigen::Matrix3d> svd;
        svd.compute(R, Eigen::ComputeFullU | Eigen::ComputeFullV);
        R = svd.matrixU() * svd.matrixV().transpose();
        assert(R.isUnitary(1e-9));
        assert(fabs(R.determinant() - 1.0) < 1.0e-6);
    }

    void ExponentialEulerTimeStepper::step3D(
        physics::RigidBody& body,
        const Eigen::Vector3d& gravity,
        const double& time_step) const
    {
        assert(body.dim() == 3);

        // Zero out velocity of fixed dof
        // Fixed dof is specified in body frame so provide a transform
        body.velocity.zero_dof(body.is_dof_fixed, body.R0);

        // Store the previous configurations and velocities
        body.pose_prev = body.pose;
        body.velocity_prev = body.velocity;

        // Update the center of mass position
        body.pose.position += time_step * body.velocity.position;

        // Update the orientaiton
        // R₁ = R₀ + h * R₀ω̂
        Eigen::Matrix3d R1 = body.pose_prev.construct_rotation_matrix();
        R1 += time_step * R1 * Eigen::Hat(body.velocity.rotation);
        project_orientation(R1);
        Eigen::AngleAxisd r1 = Eigen::AngleAxisd(R1);
        body.pose.rotation = r1.angle() * r1.axis();

        // Compute the acceleration at ( q0, v0 )
        physics::Pose<double> acceleration(
            body.force.position / body.mass,
            body.moment_of_inertia.cwiseInverse().asDiagonal()
                * body.force.rotation);
        acceleration.position += gravity;
        // Zero out acceleration of fixed dof
        // Fixed dof is specified in body frame so provide a transform
        acceleration.zero_dof(body.is_dof_fixed, body.R0);

        // Update the velocity
        body.velocity.position += time_step * acceleration.position;
        body.velocity.rotation += time_step * acceleration.rotation;
    }

} // namespace time_stepper
} // namespace ccd