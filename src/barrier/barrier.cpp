// Barrier functions that grow to infinity as x -> 0+. Includes gradient and
// hessian functions, too. These barrier functions can be used to impose
// inequlity constraints on a function.
#include <barrier/barrier.hpp>

#include <cmath>

#include <iostream>

#include <autodiff/autodiff_types.hpp>
#include <barrier/barrier_chorner.hpp>

namespace ccd {

namespace opt {

    // template spetialization
    template <> double spline_barrier<double>(double x, double s)
    {
        return barrier_horner_compensated(x, s);
    }

    double poly_log_barrier_gradient(double x, double s)
    {
        // (6*x/s**2 - 6*x**2/s**3)*log(x/s) + (-1 + 3*x**2/s**2 -
        // 2*x**3/s**3)/x
        if (x <= 0)
            return 0;

        if (x >= s)
            return 0.0;

        double x2 = x * x, s2 = s * s;
        double x3 = x2 * x, s3 = s2 * s;

        return (6 * x / s2 - 6 * x2 / s3) * log(x / s)
            + (-1 + 3 * x2 / s2 - 2 * x3 / s3) / x;
    }

    // Derivative of the spline_barrier function with respect to x.
    double spline_barrier_gradient(double x, double s)
    {
        if (x <= 0 || x >= s)
            return 0;
        double x_s = x / s;
        // g(x) = (x / s)^3 - 3 * (x / s)^2 + 3 * (x / s)
        double g = x_s * (3 + x_s * (-3 + x_s)); // Horner's method
        // g'(x) = 3 * x^2 / s^3 - 6 * x / s^2 + 3 / s
        //       = (3 * (x / s)^2 - 6 * (x / s) + 3 ) / s
        double dg = (3 + x_s * (-6 + 3 * x_s)) / s; // Horner's method
        // ϕ'(x) = -g^-2 * g'
        return -1 / (g * g) * dg;
    }

    // Second derivative of the spline_barrier function with respect to x.
    double spline_barrier_hessian(double x, double s)
    {
        if (x <= 0 || x >= s)
            return 0;
        double x_s = x / s;
        // g(x) = (x / s)^3 - 3 * (x / s)^2 + 3 * (x / s)
        double g = x_s * (3 + x_s * (-3 + x_s)); // Horner's method
        // g'(x) = 3 * x^2 / s^3 - 6 * x / s^2 + 3 / s
        //       = (3 * (x / s)^2 - 6 * (x / s) + 3 ) / s
        double dg = (3 + x_s * (-6 + 3 * x_s)) / s; // Horner's method
        // g''(x) = 6 * x / s^3 - 6 / s^2 = (6 * (x / s) - 6) / s^2
        double ddg = (6 * x_s - 6) / (s * s);
        // ϕ''(x) = 2g^-3 * (g')^2 + -g^-2 * g'' = (2 / g * (g')^2 - g'') / g^2
        return (2 / g * dg * dg - ddg) / (g * g);
    }

} // namespace opt
} // namespace ccd