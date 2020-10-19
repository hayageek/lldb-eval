/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_EXPR_GEN_H
#define INCLUDE_EXPR_GEN_H

#include <array>
#include <bitset>
#include <cstdint>
#include <random>

#include "tools/fuzzer/ast.h"

namespace fuzzer {

enum class ExprKind : unsigned char {
  EnumFirst,
  IntegerConstant = EnumFirst,
  DoubleConstant,
  VariableExpr,
  UnaryExpr,
  BinaryExpr,
  AddressOf,
  MemberOf,
  MemberOfPtr,
  ArrayIndex,
  TernaryExpr,
  BooleanConstant,
  EnumLast = TernaryExpr,
};
constexpr size_t NUM_GEN_EXPR_KINDS = (size_t)ExprKind::EnumLast + 1;

enum class TypeKind : unsigned char {
  ScalarType,
  TaggedType,
  PointerType,
  ReferenceType,
  EnumLast = ReferenceType,
};
constexpr size_t NUM_GEN_TYPE_KINDS = (size_t)TypeKind::EnumLast + 1;

class Weights {
 public:
  using ExprWeightsArray = std::array<float, NUM_GEN_EXPR_KINDS>;
  using TypeWeightsArray = std::array<float, NUM_GEN_TYPE_KINDS>;

  ExprWeightsArray& expr_weights() { return expr_weights_; }
  const ExprWeightsArray& expr_weights() const { return expr_weights_; }

  TypeWeightsArray& type_weights() { return type_weights_; }
  const TypeWeightsArray& type_weights() const { return type_weights_; }

  float& operator[](ExprKind kind) { return expr_weights_[(size_t)kind]; }
  float& operator[](TypeKind kind) { return type_weights_[(size_t)kind]; }

  const float& operator[](ExprKind kind) const {
    return expr_weights_[(size_t)kind];
  }
  const float& operator[](TypeKind kind) const {
    return type_weights_[(size_t)kind];
  }

 private:
  std::array<float, NUM_GEN_EXPR_KINDS> expr_weights_;
  std::array<float, NUM_GEN_TYPE_KINDS> type_weights_;
};

struct ExprKindWeightInfo {
  float initial_weight;
  float dampening_factor;
};

using BinOpMask = std::bitset<NUM_BIN_OPS>;
using UnOpMask = std::bitset<NUM_UN_OPS>;

struct GenConfig {
  int num_exprs_to_generate = 20;

  uint64_t int_const_min = 0;
  uint64_t int_const_max = 1000;

  double double_constant_min = 0;
  double double_constant_max = 10;

  float parenthesize_prob = 0.2f;

  float const_prob = 0.3f;
  float volatile_prob = 0.05f;

  BinOpMask bin_op_mask = ~0ull;
  UnOpMask un_op_mask = ~0ull;

  std::array<ExprKindWeightInfo, NUM_EXPR_KINDS> expr_kind_weights = {{
      {1.0f, 0.0f},  // ExprKind::IntegerConstant
      {0.0f, 0.0f},  // ExprKind::DoubleConstant
      {1.0f, 0.0f},  // ExprKind::VariableExpr
      {7.0f, 0.4f},  // ExprKind::UnaryExpr
      {3.0f, 0.4f},  // ExprKind::BinaryExpr
      {0.0f, 0.0f},  // ExprKind::AddressOf
      {0.0f, 0.0f},  // ExprKind::MemberOf
      {0.0f, 0.0f},  // ExprKind::MemberOfPtr
      {0.0f, 0.0f},  // ExprKind::ArrayIndex
      {0.0f, 0.0f},  // ExprKind::TernaryExpr
  }};
};

class GeneratorRng {
 public:
  virtual ~GeneratorRng() {}

  virtual BinOp gen_bin_op(BinOpMask mask) = 0;
  virtual UnOp gen_un_op(UnOpMask mask) = 0;
  virtual ExprKind gen_expr_kind(const Weights& array) = 0;
  virtual TypeKind gen_type_kind(const Weights& array) = 0;
  virtual uint64_t gen_u64(uint64_t min, uint64_t max) = 0;
  virtual double gen_double(double min, double max) = 0;
  virtual bool gen_parenthesize(float probability) = 0;
  virtual CvQualifiers gen_cv_qualifiers(float const_prob,
                                         float volatile_prob) = 0;
};

class DefaultGeneratorRng : public GeneratorRng {
 public:
  explicit DefaultGeneratorRng(uint32_t seed) : rng_(seed) {}

  BinOp gen_bin_op(BinOpMask mask) override;
  UnOp gen_un_op(UnOpMask mask) override;
  ExprKind gen_expr_kind(const Weights& array) override;
  TypeKind gen_type_kind(const Weights& array) override;
  uint64_t gen_u64(uint64_t min, uint64_t max) override;
  double gen_double(double min, double max) override;
  bool gen_parenthesize(float probability) override;
  CvQualifiers gen_cv_qualifiers(float const_prob,
                                 float volatile_prob) override;

 private:
  std::mt19937 rng_;
};

class ExprGenerator {
 public:
  ExprGenerator(std::unique_ptr<GeneratorRng> rng, const GenConfig& cfg)
      : rng_(std::move(rng)), cfg_(cfg) {}

  Expr generate();

 private:
  static constexpr char VAR[] = "x";

  Expr maybe_parenthesized(Expr expr);

  IntegerConstant gen_integer_constant(const Weights&);
  DoubleConstant gen_double_constant(const Weights&);
  VariableExpr gen_variable_expr(const Weights&);
  BinaryExpr gen_binary_expr(const Weights&);
  UnaryExpr gen_unary_expr(const Weights&);

  Expr gen_with_weights(const Weights&);

 private:
  std::unique_ptr<GeneratorRng> rng_;
  GenConfig cfg_;
};

}  // namespace fuzzer

#endif  // INCLUDE_EXPR_GEN_H
