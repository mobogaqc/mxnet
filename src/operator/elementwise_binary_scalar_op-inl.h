/*!
 * Copyright (c) 2015 by Contributors
 * \file elementwise_binary_op-inl.h
 * \brief Elementwise binary operation, plus, minus, mul, div
*/
#ifndef MXNET_OPERATOR_ELEMENTWISE_BINARY_SCALAR_OP_INL_H_
#define MXNET_OPERATOR_ELEMENTWISE_BINARY_SCALAR_OP_INL_H_

#include <dmlc/logging.h>
#include <mxnet/operator.h>
#include <mshadow/tensor.h>
#include <utility>
#include <string>
#include <vector>
#include <map>
#include "./operator_common.h"
#include "./mshadow_op.h"

namespace mxnet {
namespace op {

namespace elembinary {
enum ElementwiseBinaryScalarOpInputs {kLhs};
enum ElementwiseBinaryScalarOpOutputs {kOut};
enum ElementwiseBinaryScalarOpType {kPlus, kMinus, kMul, kDiv};
}  // elembinary

struct ScalarOpParam : public dmlc::Parameter<ScalarOpParam> {
    // use int for enumeration
    float scalar;
    bool scalar_on_right;
    DMLC_DECLARE_PARAMETER(ScalarOpParam) {
        DMLC_DECLARE_FIELD(scalar)
            .describe("scalar value.");
        DMLC_DECLARE_FIELD(scalar_on_right)
            .set_default(false)
            .describe("scalar operand is on the right.");
    }
};

template<typename Op>
inline elembinary::ElementwiseBinaryScalarOpType GetOpType();

template<typename Op>
inline const char* GetScalarOpTypeString();

template<>
inline elembinary::ElementwiseBinaryScalarOpType GetOpType<mshadow::op::plus>() {
  return elembinary::kPlus;
}
template<>
inline elembinary::ElementwiseBinaryScalarOpType GetOpType<mshadow::op::minus>() {
  return elembinary::kMinus;
}
template<>
inline elembinary::ElementwiseBinaryScalarOpType GetOpType<mshadow::op::mul>() {
  return elembinary::kMul;
}
template<>
inline elembinary::ElementwiseBinaryScalarOpType GetOpType<mshadow::op::div>() {
  return elembinary::kDiv;
}

template<>
inline const char* GetScalarOpTypeString<mshadow::op::plus>() {
  return "_PlusScalar";
}
template<>
inline const char* GetScalarOpTypeString<mshadow::op::minus>() {
  return "_MinusScalar";
}

template<>
inline const char* GetScalarOpTypeString<mshadow::op::mul>() {
  return "_MulScalar";
}

template<>
inline const char* GetScalarOpTypeString<mshadow::op::div>() {
  return "_DivScalar";
}

template<typename xpu, typename ForwardOp>
class ElementwiseBinaryScalarOp : public Operator {
 public:
  explicit ElementwiseBinaryScalarOp(ScalarOpParam param)
      : scalar_(param.scalar), scalar_on_right_(param.scalar_on_right) {}
  virtual void Forward(const OpContext &ctx,
                       const std::vector<TBlob> &in_data,
                       const std::vector<OpReqType> &req,
                       const std::vector<TBlob> &out_data,
                       const std::vector<TBlob> &aux_args) {
    using namespace mshadow;
    using namespace mshadow::expr;
    CHECK_EQ(in_data.size(), 1);
    CHECK_EQ(out_data.size(), 1);
    Stream<xpu> *s = ctx.get_stream<xpu>();
    Tensor<xpu, 2> lhs = in_data[elembinary::kLhs].FlatTo2D<xpu, real_t>(s);
    Tensor<xpu, 2> out = out_data[elembinary::kOut].FlatTo2D<xpu, real_t>(s);
    if (scalar_on_right_) {
      Assign(out, req[elembinary::kOut], F<ForwardOp>(scalar_, lhs));
    } else {
      Assign(out, req[elembinary::kOut], F<ForwardOp>(lhs, scalar_));
    }
  }

  virtual void Backward(const OpContext &ctx,
                        const std::vector<TBlob> &out_grad,
                        const std::vector<TBlob> &in_data,
                        const std::vector<TBlob> &out_data,
                        const std::vector<OpReqType> &req,
                        const std::vector<TBlob> &in_grad,
                        const std::vector<TBlob> &aux_args) {
    using namespace mshadow;
    using namespace mshadow::expr;
    CHECK_EQ(out_grad.size(), 1);
    CHECK(in_data.size() == 1 && in_grad.size() == 1);
    CHECK_EQ(req.size(), 1);

    Stream<xpu> *s = ctx.get_stream<xpu>();
    Tensor<xpu, 2> m_out_grad = out_grad[elembinary::kOut].FlatTo2D<xpu, real_t>(s);
    Tensor<xpu, 2> lhs_grad = in_grad[elembinary::kLhs].FlatTo2D<xpu, real_t>(s);
    switch (GetOpType<ForwardOp>()) {
      case elembinary::kPlus: {
        Assign(lhs_grad, req[elembinary::kLhs], F<mshadow_op::identity>(m_out_grad));
        break;
      }
      case elembinary::kMinus: {
        if (scalar_on_right_) {
          Assign(lhs_grad, req[elembinary::kLhs], F<mshadow_op::negation>(m_out_grad));
        } else {
          Assign(lhs_grad, req[elembinary::kLhs], F<mshadow_op::identity>(m_out_grad));
        }
        break;
      }
      case elembinary::kMul: {
        Assign(lhs_grad, req[elembinary::kLhs], scalar_ * m_out_grad);
        break;
      }
      case elembinary::kDiv: {
        Tensor<xpu, 2> lhs_data = in_data[elembinary::kLhs].FlatTo2D<xpu, real_t>(s);
        if (scalar_on_right_) {
          Assign(lhs_grad, req[elembinary::kLhs],
                 F<mshadow_op::negation>(m_out_grad * scalar_) / F<mshadow_op::square>(lhs_data));
        } else {
          Assign(lhs_grad, req[elembinary::kLhs], m_out_grad / scalar_);
        }
        break;
      }
    }
  }

 private:
    float scalar_;
    bool scalar_on_right_;
};  // class ElementwiseBinaryScalarOp


template<typename xpu>
inline Operator* CreateElementwiseBinaryScalarOp_(elembinary::ElementwiseBinaryScalarOpType type,
                                                  ScalarOpParam param) {
  switch (type) {
    case elembinary::kPlus:
      return new ElementwiseBinaryScalarOp<xpu, mshadow::op::plus>(param);
    case elembinary::kMinus:
      return new ElementwiseBinaryScalarOp<xpu, mshadow::op::minus>(param);
    case elembinary::kMul:
      return new ElementwiseBinaryScalarOp<xpu, mshadow::op::mul>(param);
    case elembinary::kDiv:
      return new ElementwiseBinaryScalarOp<xpu, mshadow::op::div>(param);
  }
  LOG(FATAL) << "uknown op type";
  return NULL;
}

// Decalre Factory function, used for dispatch specialization
template<typename xpu>
Operator* CreateElementwiseBinaryScalarOp(elembinary::ElementwiseBinaryScalarOpType type,
                                          ScalarOpParam param);

#if DMLC_USE_CXX11
template<typename ForwardOp>
class ElementwiseBinaryScalarOpProp : public OperatorProperty {
 public:
  void Init(const std::vector<std::pair<std::string, std::string> >& kwargs) override {
     param_.Init(kwargs);
  }
  std::map<std::string, std::string> GetParams() const override {
    return std::map<std::string, std::string>();
  }

  bool InferShape(std::vector<TShape> *in_shape,
                  std::vector<TShape> *out_shape,
                  std::vector<TShape> *aux_shape) const override {
    using namespace mshadow;
    CHECK_EQ(in_shape->size(), 1) << "Input:[lhs]";
    const TShape &dshape = in_shape->at(elembinary::kLhs);
    if (dshape.ndim() == 0) return false;
    out_shape->clear();
    out_shape->push_back(dshape);
    return true;
  }

  std::vector<std::string> ListArguments() const override {
    return {"lhs"};
  }

  OperatorProperty* Copy() const override {
    auto ptr = new ElementwiseBinaryScalarOpProp<ForwardOp>();
    ptr->param_ = param_;
    return ptr;
  }

  std::string TypeString() const override {
    return GetScalarOpTypeString<ForwardOp>();
  }

  // decalre dependency and inplace optimization options
  std::vector<int> DeclareBackwardDependency(
    const std::vector<int> &out_grad,
    const std::vector<int> &in_data,
    const std::vector<int> &out_data) const override {
    switch (GetOpType<ForwardOp>()) {
      case elembinary::kPlus:
      case elembinary::kMinus:
        return {out_grad[elembinary::kOut]};
      case elembinary::kMul:
      case elembinary::kDiv:
        return {out_grad[elembinary::kOut], in_data[elembinary::kLhs]};
    }
    LOG(FATAL) << "not reached";
    return {};
  }

  std::vector<std::pair<int, void*> > BackwardInplaceOption(
    const std::vector<int> &out_grad,
    const std::vector<int> &in_data,
    const std::vector<int> &out_data,
    const std::vector<void*> &in_grad) const override {
    switch (GetOpType<ForwardOp>()) {
      case elembinary::kPlus:
      case elembinary::kMinus:
        return {};
      case elembinary::kMul:
      case elembinary::kDiv:
        return {{out_grad[elembinary::kOut], in_grad[elembinary::kLhs]}};
    }
    LOG(FATAL) << "not reached";
    return {};
  }

  std::vector<std::pair<int, void*> > ForwardInplaceOption(
    const std::vector<int> &in_data,
    const std::vector<void*> &out_data) const override {
    return {{in_data[elembinary::kLhs], out_data[elembinary::kOut]}};
  }

  Operator* CreateOperator(Context ctx) const override;

 private:
    ScalarOpParam param_;
};
#endif  // DMLC_USE_CXX11
}  // namespace op
}  // namespace mxnet
#endif  // MXNET_OPERATOR_ELEMENTWISE_BINARY_SCALAR_OP_INL_H_
