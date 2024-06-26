#include <iostream>
#include <fstream>

#include "Solver.hpp"


RUVP Q2var(const Eigen::Vector4d &Q) {
    RUVP var{};
    var.r = Q[0];
    var.u = Q[1] / Q[0];
    var.v = Q[2] / Q[0];
    var.p = (Q[3] - var.r * (var.u * var.u + var.v * var.v) / 2) * (GAMMA - 1);
    return var;
}

auto Q2F(const Eigen::Vector4d &Q) {
    Eigen::Vector4d F(4);
    double u = Q[1] / Q[0];
    double v = Q[2] / Q[0];
    double p = (Q[3] - Q[0] * (u * u + v * v) / 2) * (GAMMA - 1);
    F[0] = Q[1];
    F[1] = Q[0] * u * u + p;
    F[2] = Q[0] * u * v;
    F[3] = u * (Q[3] + p);
    return F;
}

auto Q2G(const Eigen::Vector4d &Q) {
    Eigen::Vector4d G(4);
    double u = Q[1] / Q[0];
    double v = Q[2] / Q[0];
    double p = (Q[3] - Q[0] * (u * u + v * v) / 2) * (GAMMA - 1);
    G[0] = Q[2];
    G[1] = Q[0] * u * v;
    G[2] = Q[0] * v * v + p;
    G[3] = v * (Q[3] + p);
    return G;
}

auto var2Q(const RUVP &var) {
    Eigen::Vector4d Q;
    Q[0] = var.r;
    Q[1] = var.r * var.u;
    Q[2] = var.r * var.v;
    Q[3] = var.r * (var.u * var.u + var.v * var.v) / 2 + var.p / (GAMMA - 1);
    return Q;
}

Solver::Solver(const std::vector<std::pair<double, double>> &mesh, const std::vector<RUVP> &init_solution,
               double dt, int N_x, double courant) : xy_(mesh), solution_(init_solution), dt_(dt), stride_(N_x),
                                                     N_y(mesh.size() / N_x), time_(0) {
    Flux_x.resize((stride_ - 1) * (N_y - 2));
    Flux_y.resize((stride_ - 2) * (N_y - 1));
    Q_.resize(xy_.size());
    F_.resize(xy_.size());
    G_.resize(xy_.size());
    for (int i = 0; i < mesh.size(); i++) {
        Q_[i] = var2Q(solution_[i]);
        F_[i] = Q2F(Q_[i]);
        G_[i] = Q2G(Q_[i]);
    }
    x_step_ = xy_[1].first - xy_[0].first;
    y_step_ = xy_[ij2k(0, 1)].second - xy_[0].second;
    cour_ = courant;
}

void Solver::boundaries() {
    for (int i = 0; i < stride_; i++) {
        Q_[ij2k(i, 0)] = Q_[ij2k(i, 1)];
        Q_[ij2k(i, 0)][2] = -Q_[ij2k(i, 0)][2];
        Q_[ij2k(i, static_cast<int>(N_y - 1))] = Q_[ij2k(i, static_cast<int>(N_y - 2))];
        Q_[ij2k(i, static_cast<int>(N_y - 1))][2] = -Q_[ij2k(i, static_cast<int>(N_y - 1))][2];
    }
    for (int j = 0; j < N_y; j++) {
        Q_[ij2k(0, j)] = Q_[ij2k(1, j)];
        Q_[ij2k(0, j)][1] = -Q_[ij2k(0, j)][1];
        Q_[ij2k(static_cast<int>(stride_ - 1), j)] = Q_[ij2k(static_cast<int>(N_y - 2), j)];
        Q_[ij2k(static_cast<int>(N_y - 1), j)][1] = -Q_[ij2k(static_cast<int>(N_y - 1), j)][1];
    }
}

void Solver::LaxFriedrichs(const double T_end) {
    std::vector<Eigen::Vector4d> Q_tmp = Q_;
    auto cfl = [&](int idx) {
        auto [r, u, v, p] = Q2var(Q_[idx]);
        auto a = std::sqrt(GAMMA * p / r);
        if (dt_ > cour_ / ((abs(u) + a) / x_step_ + (abs(v) + a) / y_step_)) {
            dt_ = cour_ / ((abs(u) + a) / x_step_ + (abs(v) + a) / y_step_);
        };
    };
    int cnt = 0;
    while (time_ < T_end) {
        for (int i = 0; i < xy_.size(); i++) {
            cfl(i);
        }
        for (int i = 1; i < stride_ - 1; i++) {
            for (int j = 1; j < N_y - 1; j++) {
                Q_tmp[ij2k(i, j)] = (Q_[ij2k(i + 1, j)] + Q_[ij2k(i, j - 1)] + Q_[ij2k(i - 1, j)] +
                                     Q_[ij2k(i, j + 1)]) / 4 - ((F_[ij2k(i + 1, j)] - F_[ij2k(i - 1, j)]) / x_step_ +
                                                                (G_[ij2k(i, j + 1)] - G_[ij2k(i, j - 1)]) / y_step_) *
                                                               dt_ / 2;
            }
        }
        Q_ = Q_tmp;
        boundaries();
        for (int i = 0; i < solution_.size(); i++) {
            F_[i] = Q2F(Q_[i]);
            G_[i] = Q2G(Q_[i]);
        }
        time_ += dt_;
        for (int i = 0; i < Q_.size(); i++) {
            solution_[i] = Q2var(Q_[i]);
        }
//        std::ostringstream filename;
//        filename << std::setw(4) << std::setfill('0') << cnt++ << ".vtk";
//        VisualVTK(std::string("../pictures/lax_") + filename.str());
        std::cout << "Time = " << time_ << std::endl;
    }
    for (int i = 0; i < xy_.size(); i++) {
        solution_[i] = Q2var(Q_[i]);
    }
}


void Solver::VisualVTK(const std::string &file_name) {
    std::ofstream out(file_name);
    out << "# vtk DataFile Version 3.0\nvtk output\nASCII\nDATASET RECTILINEAR_GRID\n";
    out << "DIMENSIONS " << stride_ - 1 << " " << N_y - 1 << " 1" << std::endl;
    out << "X_COORDINATES " << stride_ - 1 << " float\n";
    for (int i = 1; i < stride_; i++) {
        out << xy_[i].first - x_step_ / 2 << " ";
    }
    out << std::endl;
    out << "Y_COORDINATES " << N_y - 1 << " float\n";
    for (int i = 1; i < N_y; i++) {
        out << xy_[ij2k(0, i)].second - y_step_ / 2 << " ";
    }
    out << std::endl;
    out << "Z_COORDINATES 1 float\n0\n";
    out << "CELL_DATA " << (stride_ - 2) * (N_y - 2) << std::endl;
    out << "SCALARS density double 1" << std::endl;
    out << "LOOKUP_TABLE default" << std::endl;
    for (int j = 1; j < N_y - 1; j++) {
        for (int i = 1; i < stride_ - 1; i++) {
            out << solution_[ij2k(i, j)].r << std::endl;
        }
    }
    out << "SCALARS pressure double 1" << std::endl;
    out << "LOOKUP_TABLE default" << std::endl;
    for (int j = 1; j < N_y - 1; j++) {
        for (int i = 1; i < stride_ - 1; i++) {
            out << solution_[ij2k(i, j)].p << std::endl;
        }
    }
    out << "VECTORS velocity double" << std::endl;
    for (int j = 1; j < N_y - 1; j++) {
        for (int i = 1; i < stride_ - 1; i++) {
            out << std::fixed << solution_[ij2k(i, j)].u << " " << solution_[ij2k(i, j)].v << " 0" << std::endl;
        }
    }
    out.close();
}

void Solver::VisualVTK_FULL(const std::string &file_name) {
    std::ofstream out(file_name);
    out << "# vtk DataFile Version 3.0\nvtk output\nASCII\nDATASET RECTILINEAR_GRID\n";
    out << "DIMENSIONS " << stride_ + 1 << " " << N_y + 1 << " 1" << std::endl;
    out << "X_COORDINATES " << stride_ + 1 << " float\n";
    for (int i = 0; i < stride_; i++) {
        out << xy_[i].first - x_step_ / 2 << " ";
    }
    out << xy_[stride_ - 1].first + x_step_ / 2 << std::endl;
    out << "Y_COORDINATES " << N_y + 1 << " float\n";
    for (int i = 0; i < N_y; i++) {
        out << xy_[ij2k(0, i)].second - y_step_ / 2 << " ";
    }
    out << xy_[ij2k(static_cast<std::size_t>(0), N_y - 1)].second + y_step_ / 2 << std::endl;
    out << "Z_COORDINATES 1 float\n0\n";
    out << "CELL_DATA " << stride_ * N_y << std::endl;
    out << "SCALARS density double 1" << std::endl;
    out << "LOOKUP_TABLE default" << std::endl;
    for (int j = 0; j < N_y; j++) {
        for (int i = 0; i < stride_; i++) {
            out << solution_[ij2k(i, j)].r << std::endl;
        }
    }
    out << "SCALARS pressure double 1" << std::endl;
    out << "LOOKUP_TABLE default" << std::endl;
    for (int j = 0; j < N_y; j++) {
        for (int i = 0; i < stride_; i++) {
            out << solution_[ij2k(i, j)].p << std::endl;
        }
    }
    out << "VECTORS velocity double" << std::endl;
    for (int j = 0; j < N_y; j++) {
        for (int i = 0; i < stride_; i++) {
            out << std::fixed << solution_[ij2k(i, j)].u << " " << solution_[ij2k(i, j)].v << " 0" << std::endl;
        }
    }
    out.close();
}

double Solver::compare(const std::vector<RUVP> &expected) {
    double res = 0.0;
    for (int i = 0; i < expected.size(); i++) {
        res += std::abs(expected[i].p - solution_[i].p);
        res += std::abs(expected[i].u - solution_[i].u);
        res += std::abs(expected[i].v - solution_[i].v);
        res += std::abs(expected[i].r - solution_[i].r);
    }
    return res / static_cast<double>(expected.size());
}
