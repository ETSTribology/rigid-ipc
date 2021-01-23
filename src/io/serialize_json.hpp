#pragma once

#include <Eigen/Dense>
#include <nlohmann/json.hpp>
#include <utils/eigen_ext.hpp>

namespace ccd {
namespace io {

    template <typename T, int dim, int max_dim = dim>
    void from_json(
        const nlohmann::json& json,
        Eigen::Matrix<T, dim, 1, Eigen::ColMajor, max_dim, 1>& vector);

    template <typename Derived>
    void
    from_json(const nlohmann::json& json, Eigen::MatrixBase<Derived>& matrix);

    template <>
    void
    from_json<bool>(const nlohmann::json& json, Eigen::VectorX<bool>& vector);

    template <typename Derived>
    nlohmann::json to_json(const Eigen::MatrixBase<Derived>& matrix);

    nlohmann::json to_json_string(
        const Eigen::MatrixXd& matrix, const std::string& format = ".16e");

} // namespace io
} // namespace ccd

#include "serialize_json.tpp"
