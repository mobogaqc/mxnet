/*!
 *  Copyright (c) 2015 by Contributors
 * \file activation_op-inl.hpp
 * \brief activation operator of mxnet
 */

#ifndef ACTIVATION_OP_INL_HPP
#define ACTIVATION_OP_INL_HPP
#pragma once
#include <mxnet/operator.h>

namespace mxnet {
template<typename xpu, typename ForwardOp, typename BackOp>
class ActivationOp : public Operator {
public:
  virtual void InferShape(const std::vector<TShape> &in_shape,
                          std::vector<TShape> *out_shape) {
    CHECK(in_shape.size() == 1) << "Activation Op: only 1 input is allowed";
    out_shape->resize(in_shape.size());
    out_shape->at(0) = in_shape[0];
  }
  virtual void Forward(Option opt,
                       RunContext ctx,
                       const std::vector<TBlob> &in_data,
                       const std::vector<TBlob> &out_data) {
    CHECK(out_data.size() == 1) << "Activation Op: only 1 output data is allowed";
    CHECK(in_data.size() == 1) << "Activation Op: only 1 input data is allowed";
    mshadow::Stream<xpu> *stream = static_cast<mshadow::Stream<xpu> *>(ctx.stream);
    mshadow::Tensor<xpu, 2> in = in_data[0].FlatTo2D(stream);
    mshadow::Tensor<xpu, 2> out = out_data[0].FlatTo2D(stream);
    out = mshadow::expr::F<ForwardOp>(in);
  }
  virtual void Backward(RunContext ctx,
                        const std::vector<TBlob> &grad_next,
                        const std::vector<TBlob> &in_data,
                        const std::vector<TBlob> &out_grad,
                        const std::vector<GradReqType> req) {
    CHECK(grad_next.size() == 1) << "Activation Op: only 1 input grad is allowed";
    CHECK(in_data.size() == 1) << "Activation Op: only 1 input data is allowed";
    CHECK(req.size() == 1) << "Activation Op: only 1 req is allowed";
    CHECK(req[0] == kWriteInplace) << "Activation Op: only support inplace mode";
    mshadow::Stream<xpu> *stream = static_cast<mshadow::Stream<xpu> *>(ctx.stream);
    mshadow::Tensor<xpu, 2> grad = grad_next[0].FlatTo2D(stream);
    mshadow::Tensor<xpu, 2> data = in_data[0].FlatTo2D(stream);
    data = mshadow::expr::F<BackOp>(data) * grad;
  }
}; // class ActivationOp
} // namespace cxxnet

#endif // ACTIVATION_OP_INL_HPP
