// Solve the optimization general_problem using Newton's Method with barriers
// for the constraints.
#include "barrier_problem.hpp"

#include <finitediff.hpp>

namespace ccd {
namespace opt {

    // Compute the objective function f(x) = E(x) + κ ∑_{k ∈ C} b(d(x_k))
    double BarrierProblem::compute_objective(
        const Eigen::VectorXd& x,
        Eigen::VectorXd& grad,
        Eigen::SparseMatrix<double>& hess,
        bool compute_grad,
        bool compute_hess)
    {
        Eigen::VectorXd grad_Ex, grad_Bx;
        Eigen::SparseMatrix<double> hess_Ex, hess_Bx;

        // Compute each term
        double Ex = compute_energy_term(
            x, grad_Ex, hess_Ex, compute_grad, compute_hess);
        int num_constraints;
        double Bx = compute_barrier_term(
            x, grad_Bx, hess_Bx, num_constraints, compute_grad, compute_hess);

        double kappa = get_barrier_stiffness();

        if (compute_grad) {
            grad = grad_Ex + kappa * grad_Bx;
        }
        if (compute_hess) {
            hess = hess_Ex + kappa * hess_Bx;
        }
        return Ex + kappa * Bx;
    }

    Eigen::VectorXd
    eval_grad_energy_approx(BarrierProblem& problem, const Eigen::VectorXd& x)
    {
        auto f = [&](const Eigen::VectorXd& xk) -> double {
            return problem.compute_energy_term(xk);
        };
        Eigen::VectorXd grad(x.size());
        fd::finite_gradient(x, f, grad);
        return grad;
    }

    Eigen::MatrixXd
    eval_hess_energy_approx(BarrierProblem& problem, const Eigen::VectorXd& x)
    {
        Eigen::MatrixXd hess;
        auto func_grad_f = [&](const Eigen::VectorXd& xk) -> Eigen::VectorXd {
            Eigen::VectorXd grad;
            problem.compute_energy_term(xk, grad);
            return grad;
        };

        fd::finite_jacobian(x, func_grad_f, hess, fd::AccuracyOrder::SECOND);
        return hess;
    }

} // namespace opt
} // namespace ccd