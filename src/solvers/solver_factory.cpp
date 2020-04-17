#include "solver_factory.hpp"

#include <solvers/barrier_solver.hpp>
#include <solvers/ipc_solver.hpp>

namespace ccd {
namespace opt {

    const SolverFactory& SolverFactory::factory()
    {
        static SolverFactory instance;

        return instance;
    }

    SolverFactory::SolverFactory()
    {
        barrier_solvers.emplace(
            BarrierSolver::solver_name(), std::make_shared<BarrierSolver>());
        barrier_solvers.emplace(
            IPCSolver::solver_name(), std::make_shared<IPCSolver>());
    }

    std::shared_ptr<OptimizationSolver>
    SolverFactory::get_barrier_solver(const std::string& solver_name) const
    {
        auto it = barrier_solvers.find(solver_name);
        assert(it != barrier_solvers.end());
        return it->second;
    }

} // namespace opt
} // namespace ccd
