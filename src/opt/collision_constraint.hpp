#pragma once

#include <Eigen/SparseCore>

#include <ccd/collision_detection.hpp>
#include <utils/not_implemented_error.hpp>

#include <autodiff/autodiff_types.hpp>

#include <memory>

namespace ccd {
namespace opt {

    /// @brief Generic type 2×1 vector
    template <typename T> using Vector2T = Eigen::Matrix<T, 2, 1>;
    typedef Eigen::Matrix<DScalar, Eigen::Dynamic, 1> VectorXDS;

    template <typename T> struct ImpactTData {
        std::array<Eigen::Vector2d, 4> v;
        std::array<Vector2T<T>, 4> u;
        size_t impacting_side; ///< @brief 0 or 1, indicates node of edge_23
        size_t impacting_index() const { return 2 + impacting_side; }
    };

    class DoubleTriplet : public Eigen::Triplet<double> {
    public:
        DoubleTriplet(const int& i, const int& j, const double& v)
            : Eigen::Triplet<double>(i, j, v)
        {
        }
        void set_row(const int& i) { m_row = i; }
    };

    class CollisionConstraint {
    public:
        CollisionConstraint();
        virtual ~CollisionConstraint();

        virtual void initialize(const Eigen::MatrixX2d& vertices,
            const Eigen::MatrixX2i& edges, const Eigen::MatrixXd& Uk);

        virtual void detectCollisions(const Eigen::MatrixXd& Uk);

        // Assembly of global Matrices
        // -------------------------------------------------------------------
        void assemble_hessian(const std::vector<DScalar>& constraints,
            std::vector<Eigen::SparseMatrix<double>>& hessian);

        void assemble_jacobian(
            const std::vector<DScalar>& constraints, Eigen::MatrixXd& jacobian);

        void assemble_jacobian(const std::vector<DScalar>& constraints,
            Eigen::SparseMatrix<double>& jacobian);

        void assemble_jacobian_triplets(const std::vector<DScalar>& constraints,
            std::vector<DoubleTriplet>& jacobian);

        void assemble_constraints(
            const std::vector<DScalar>& constraints, Eigen::VectorXd& g_uk);

        // Helper Functions
        // -------------------------------------------------------------
        void get_impact_nodes(const EdgeEdgeImpact& impact, int nodes[4]);

        template <typename T>
        bool compute_toi_alpha(
            const ImpactTData<T>& data, T& toi, T& alpha_ij, T& aplha_kl);

        template <typename T>
        ImpactTData<T> get_impact_data(const Eigen::MatrixXd& displacements,
            const EdgeEdgeImpact ee_impact);

        // Pure virtual functions
        // ------------------------------------------------------------
        virtual int number_of_constraints() = 0;
        virtual void compute_constraints(
            const Eigen::MatrixXd& Uk, Eigen::VectorXd& g_uk)
            = 0;

        virtual void compute_constraints_jacobian(
            const Eigen::MatrixXd& Uk, Eigen::MatrixXd& g_uk_jacobian)
            = 0;

        virtual void compute_constraints_jacobian(const Eigen::MatrixXd& /*Uk*/,
            Eigen::SparseMatrix<double>& /*g_uk_jacobian*/)
        {
            throw NotImplementedError("compute_constraints_jacobian not implemented");
        }

        virtual void compute_constraints_hessian(const Eigen::MatrixXd& Uk,
            std::vector<Eigen::SparseMatrix<double>>& g_uk_hessian)
            = 0;

        virtual void compute_constraints_and_derivatives(
            const Eigen::MatrixXd& /*Uk*/, Eigen::VectorXd& /*g_uk*/,
            Eigen::MatrixXd& /*g_uk_jacobian*/,
            std::vector<Eigen::SparseMatrix<double>>& /*g_uk_hessian*/)
        {
            throw NotImplementedError("compute_constraints_and_derivatives not implemented");
        }

        virtual void compute_constraints(const Eigen::MatrixXd& /*Uk*/,
            Eigen::VectorXd& /*g_uk*/,
            Eigen::SparseMatrix<double>& /*g_uk_jacobian*/,
            Eigen::VectorXi& /*g_uk_active*/)
        {
            throw NotImplementedError("compute_constraints not implemented");
        }
        // Settings
        // ----------
        DetectionMethod detection_method;
        bool update_collision_set;
        bool extend_collision_set;
        bool recompute_toi;

        // Structures used for detection
        // ------------
        /// @brief All edge-vertex contact
        EdgeVertexImpacts ev_impacts;

        /// @brief All edge-edge contact
        EdgeEdgeImpacts ee_impacts;

        /// @brief #E,1 indices of the edges' first impact
        Eigen::VectorXi edge_impact_map;

        /// @brief The current number of pruned impacts
        int num_pruned_impacts;

        std::shared_ptr<const Eigen::MatrixX2d> vertices;
        std::shared_ptr<const Eigen::MatrixX2i> edges;
    };

    void slice_vector(const Eigen::MatrixX2d& data, const Eigen::Vector2i e_ij,
        Eigen::Vector2i e_kl, std::array<Eigen::Vector2d, 4>& d);

    template <typename T> bool differentiable();

} // namespace opt
} // namespace ccd