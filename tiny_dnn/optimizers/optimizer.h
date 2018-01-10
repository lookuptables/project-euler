/*
    Copyright (c) 2013, Taiga Nomi and the respective contributors
    All rights reserved.

    Use of this source code is governed by a BSD-style license that can be found
    in the LICENSE file.
*/
#pragma once

#include <algorithm>
#include <unordered_map>

#include "tiny_dnn/util/util.h"

namespace tiny_dnn {

/**
 * base class of optimizer
 * usesHessian : true if an optimizer uses hessian (2nd order derivative of loss
 *function)
 **/
struct optimizer {
  optimizer()                  = default;
  optimizer(const optimizer &) = default;
  optimizer(optimizer &&)      = default;
  optimizer &operator=(const optimizer &) = default;
  optimizer &operator=(optimizer &&) = default;
  virtual ~optimizer()               = default;
  virtual void update(const vec_t &dW, vec_t &W, bool parallelize) = 0;
  virtual void reset() {}  // override to implement pre-learning action
};

// helper class to hold N values for each weight
template <int N, int M = 0>
struct stateful_optimizer : public optimizer {
    void reset() override {
        for (auto &e : E_) e.clear();
    }

   protected:
    template <int Index>
    vec_t &get(const vec_t &key) {
        static_assert(Index < N, "index out of range");
        if (E_[Index][&key].empty()) E_[Index][&key].resize(key.size(), float_t());
        return E_[Index][&key];
    }

    template <int Index>
    vec_t &getP(const vec_t &key) {
        static_assert(Index < N, "index out of range");
        if (P_[Index][&key].empty()) P_[Index][&key].resize(key.size(), float_t());
        return P_[Index][&key];
    }

    template <int Index>
    double& getF(const vec_t &key) {
        static_assert(Index < M, "index out of range");
        return F_[Index][&key];
    }

    std::unordered_map<const vec_t *, vec_t> E_[N];
    std::unordered_map<const vec_t *, vec_t> P_[N];
    std::unordered_map<const vec_t *, double> F_[M];
};

/**
 * adaptive gradient method
 *
 * J Duchi, E Hazan and Y Singer,
 * Adaptive subgradient methods for online learning and stochastic optimization
 * The Journal of Machine Learning Research, pages 2121-2159, 2011.
 **/
struct adagrad : public stateful_optimizer<1> {
  adagrad() : alpha(float_t(0.01)), eps(float_t(1e-8)) {}

  void update(const vec_t &dW, vec_t &W, bool parallelize) {
    vec_t &g = get<0>(W);
    for_i(parallelize, W.size(), [&](size_t i) {
      g[i] += dW[i] * dW[i];
      W[i] -= alpha * dW[i] / (std::sqrt(g[i]) + eps);
    });
  }

  float_t alpha;  // learning rate
 private:
  float_t eps;
};

struct avgadagrad : public stateful_optimizer<2> {
  avgadagrad() : alpha(float_t(0.01)), eps(float_t(1e-8)), count(0) {}

  void update(const vec_t &dW, vec_t &W, bool parallelize) {
    vec_t g = get<0>(W);
    auto& sum = get<1>(W);
    for_i(parallelize, W.size(), [&](size_t i) {
      g[i] += dW[i] * dW[i];
      W[i] -= alpha * dW[i] / (std::sqrt(g[i]) + eps);
    });

    if (count == 0) {
        sum = g;
    } else {
        for (size_t i = 0; i < g.size(); ++i) {
            sum[i] += g[i];
        }
    }
    ++count;
    get<0>(W) = sum;
    for (auto& x : get<0>(W)) {
        x /= count;
    }
  }

  float_t alpha;  // learning rate
 private:
  float_t eps;
  size_t count;
};

/**
 * RMSprop
 *
 * T Tieleman, and G E Hinton,
 * Lecture 6.5 - rmsprop, COURSERA: Neural Networks for Machine Learning (2012)
 **/
struct rms_prop : public stateful_optimizer<1> {
  rms_prop() : alpha(float_t(0.0001)), mu(float_t(0.99)), eps(float_t(1e-8)) {}

  void update(const vec_t &dW, vec_t &W, bool parallelize) {
    vec_t &g = get<0>(W);

    for_i(parallelize, W.size(), [&](size_t i) {
      g[i] = mu * g[i] + (1 - mu) * dW[i] * dW[i];
      W[i] -= alpha * dW[i] / std::sqrt(g[i] + eps);
    });
  }

  float_t alpha;  // learning rate
  float_t mu;     // decay term
 private:
  float_t eps;  // constant value to avoid zero-division
};

/**
 * @brief [a new optimizer (2015)]
 * @details [see Adam: A Method for Stochastic Optimization (Algorithm 1)
 *               http://arxiv.org/abs/1412.6980]
 *
 */
struct adam : public stateful_optimizer<2> {
  adam()
    : alpha(float_t(0.001)),
      b1(float_t(0.9)),
      b2(float_t(0.999)),
      b1_t(float_t(0.9)),
      b2_t(float_t(0.999)),
      eps(float_t(1e-8)) {}

  void update(const vec_t &dW, vec_t &W, bool parallelize) {
    vec_t &mt = get<0>(W);
    vec_t &vt = get<1>(W);

    for_i(parallelize, W.size(), [&](size_t i) {
      mt[i] = b1 * mt[i] + (float_t(1) - b1) * dW[i];
      vt[i] = b2 * vt[i] + (float_t(1) - b2) * dW[i] * dW[i];

      // L2 norm based update rule
      W[i] -= alpha * (mt[i] / (float_t(1) - b1_t)) /
              std::sqrt((vt[i] / (float_t(1) - b2_t)) + eps);
    });

    b1_t *= b1;
    b2_t *= b2;
  }

  float_t alpha;  // learning rate
  float_t b1;     // decay term
  float_t b2;     // decay term
  float_t b1_t;   // decay term power t
  float_t b2_t;   // decay term power t

 private:
  float_t eps;  // constant value to avoid zero-division
};

/**
 * @brief [a new optimizer (2015)]
 * @details [see Adam: A Method for Stochastic Optimization (Algorithm 2)
 *               http://arxiv.org/abs/1412.6980]
 *
 */
struct adamax : public stateful_optimizer<2> {
  adamax()
    : alpha(float_t(0.002)),
      b1(float_t(0.9)),
      b2(float_t(0.999)),
      b1_t(b1),
      eps(float_t(1e-8)) {}

  void update(const vec_t &dW, vec_t &W, bool parallelize) {
    vec_t &mt = get<0>(W);
    vec_t &ut = get<1>(W);

    for_i(parallelize, W.size(), [&](int i) {
      mt[i] = b1 * mt[i] + (float_t(1) - b1) * dW[i];
      ut[i] = std::max(b2 * ut[i], std::abs(dW[i]));

      // Lp norm based update rule
      W[i] -= (alpha / (1.0 - b1_t)) * (mt[i] / (ut[i] + eps));
    });

    b1_t *= b1;
  }

  float_t alpha;  // learning rate
  float_t b1;     // decay term
  float_t b2;     // decay term
  float_t b1_t;   // decay term power t

 private:
  float_t eps;  // constant value to avoid zero-division
};

/**
 * SGD without momentum
 *
 * slightly faster than tiny_dnn::momentum
 **/
struct gradient_descent : public optimizer {
  gradient_descent() : alpha(float_t(0.01)), lambda(float_t(0)) {}

  void update(const vec_t &dW, vec_t &W, bool parallelize) {
      /*
      EVERY_MS(
          {
              for (size_t i = 0; i < W.size(); ++i) {
                  printf("%d %lf %lf %lf\n", (int)i, (double)W[i], (double)dW[i], (double)alpha);
              }
          },
          1000);
      */
      for_i(parallelize, W.size(), [&](size_t i) { W[i] = W[i] - alpha * (dW[i] + lambda * W[i]); });
  }

  float_t alpha;   // learning rate
  float_t lambda;  // weight decay
};

struct avg_gradient_descent : public stateful_optimizer<1, 1> {
  avg_gradient_descent() : alpha(float_t(0.01)), lambda(float_t(0)) {}

  void update(const vec_t &dW, vec_t &W, bool parallelize) {
      auto& sum = getP<0>(W);
      auto& count = getF<0>(W);

      EVERY_MS(
          {
              for (size_t i = 0; i < W.size(); ++i) {
                  printf("%d %lf %lf %lf %lf %lf\n", (int)i, (double)W[i], (double)dW[i], (double)alpha,
                         double((i < sum.size()) ? sum[i] : 0), count);
              }
              printf("------------------------\n");
              fflush(stdout);
          },
          10000);
      for_i(parallelize, W.size(), [&](size_t i) { W[i] = W[i] - alpha * (dW[i] + lambda * W[i]); });

      if (0 == count) {
          sum = W;
          count = 1.0;
      } else {
          static constexpr double kMul = 0.99;
          for (size_t i = 0; i < W.size(); ++i) {
              sum[i] = sum[i]*kMul + W[i];
          }
          count = count*kMul + 1.0;
      }

      /*
      if (count > 16) {
          W = result(dW, W, parallelize);
          resetAvg(dW, W, parallelize);
      }
      */
  }

  void resetAvg(const vec_t &dW, vec_t &W, bool parallelize) {
    auto& sum = get<0>(W);
    for (auto& x: sum) {
        x = 0;
    }
    auto& count = getF<0>(W);
    count = 0;
  }

  vec_t result(const vec_t &dW, vec_t &W, bool parallelize) {
    auto sum = get<0>(W);
    auto count = getF<0>(W);
    for (auto& x: sum) {
        x /= count;
    }
    return sum;
  }

  float_t alpha;   // learning rate
  float_t lambda;  // weight decay
};

/**
 * SGD with momentum
 *
 * B T Polyak,
 * Some methods of speeding up the convergence of iteration methods
 * USSR Computational Mathematics and Mathematical Physics, 4(5):1-17, 1964.
 **/
struct momentum : public stateful_optimizer<1> {
 public:
  momentum() : alpha(float_t(0.01)), lambda(float_t(0)), mu(float_t(0.9)) {}

  void update(const vec_t &dW, vec_t &W, bool parallelize) {
    vec_t &dWprev = get<0>(W);

    for_i(parallelize, W.size(), [&](size_t i) {
      float_t V = mu * dWprev[i] - alpha * (dW[i] + W[i] * lambda);
      W[i] += V;
      dWprev[i] = V;
    });
  }

  float_t alpha;   // learning rate
  float_t lambda;  // weight decay
  float_t mu;      // momentum
};

/**
 * SGD with Nesterov momentum
 *
 * Y Nesterov,
 * A method for unconstrained convex minimization problem with the rate of
 * convergence o(1/k2), Doklady ANSSSR, vol.269, pp.543-547, 1983.
 **/
struct nesterov_momentum : public stateful_optimizer<1> {
 public:
  nesterov_momentum()
    : alpha(float_t(0.01)), lambda(float_t(0)), mu(float_t(0.9)) {}

  void update(const vec_t &dW, vec_t &W, bool parallelize) {
    vec_t &dWprev = get<0>(W);

    for_i(parallelize, W.size(), [&](size_t i) {
      float_t V = mu * dWprev[i] - alpha * (dW[i] + W[i] * lambda);
      W[i] += (-mu) * dWprev[i] + (1 + mu) * V;
      dWprev[i] = V;
    });
  }

  float_t alpha;   // learning rate
  float_t lambda;  // weight decay
  float_t mu;      // momentum
};

struct avg_nesterov_momentum : public stateful_optimizer<2> {
 public:
  avg_nesterov_momentum()
    : alpha(float_t(0.01)), lambda(float_t(0)), mu(float_t(0.9)), count(0) {}

  void update(const vec_t &dW, vec_t &W, bool parallelize) {
    vec_t dWprev = get<0>(W);
    auto& sum = get<1>(W);

    for_i(parallelize, W.size(), [&](size_t i) {
      float_t V = mu * dWprev[i] - alpha * (dW[i] + W[i] * lambda);
      W[i] += (-mu) * dWprev[i] + (1 + mu) * V;
      dWprev[i] = V;
    });

    if (count == 0) {
        sum = dWprev;
    } else {
        for (size_t i = 0; i < dWprev.size(); ++i) {
            sum[i] += dWprev[i];
        }
    }
    ++count;
    get<0>(W) = sum;
    for (auto& x : get<0>(W)) {
        x /= count;
    }
  }

  float_t alpha;   // learning rate
  float_t lambda;  // weight decay
  float_t mu;      // momentum

private:
  size_t count;
};

}  // namespace tiny_dnn
