/*!
 * Copyright (c) 2015 by Contributors
 * \file softmax.cc
 * \brief
 * \author Bing Xu
*/

#include <mxnet/registry.h>
#include "./softmax-inl.h"

namespace mxnet {
namespace op {
template<>
Operator *CreateOp<cpu>(SoftmaxParam param) {
  return new SoftmaxOp<cpu>(param);
}

Operator *SoftmaxProp::CreateOperator(Context ctx) const {
  DO_BIND_DISPATCH(CreateOp, param_);
}

DMLC_REGISTER_PARAMETER(SoftmaxParam);

REGISTER_OP_PROPERTY(Softmax, SoftmaxProp);
}  // namespace op
}  // namespace mxnet

