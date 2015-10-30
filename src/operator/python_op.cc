/*!
 * Copyright (c) 2015 by Contributors
 * \file python_op.cc
 * \brief
 * \author Junyuan Xie
*/
#include "./python_op-inl.h"

namespace mxnet {
namespace op {
template<>
Operator *CreateOp<cpu>(PythonOpParam param) {
  return new PythonOp<cpu>(param);
}

Operator* PythonOpProp::CreateOperator(Context ctx) const {
  DO_BIND_DISPATCH(CreateOp, param_);
}

DMLC_REGISTER_PARAMETER(PythonOpParam);

MXNET_REGISTER_OP_PROPERTY(Python, PythonOpProp)
.describe("Stub for calling an operator implemented in python.")
.add_arguments(PythonOpParam::__FIELDS__())
.set_key_var_num_args("num_args");

}  // namespace op
}  // namespace mxnet
