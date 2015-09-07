// Copyright (c) 2015 by Contributors
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE

#include <mxnet/io.h>
#include <dmlc/registry.h>
#include "./image_augmenter.h"
#include "./iter_batch.h"

// Registers
namespace dmlc {
DMLC_REGISTRY_ENABLE(::mxnet::DataIteratorReg);
}  // namespace dmlc

namespace mxnet {
namespace io {
// Register parameters in header files
DMLC_REGISTER_PARAMETER(BatchParam);
DMLC_REGISTER_PARAMETER(ImageAugmentParam);
}  // namespace io
}  // namespace mxnet
