/*!
 *  Copyright (c) 2015 by Contributors
 * \file iter_batchloader.h
 * \brief define a batch adapter to create tblob batch 
 */
#ifndef MXNET_IO_ITER_BATCHLOADER_H_
#define MXNET_IO_ITER_BATCHLOADER_H_

#include <mxnet/io.h>
#include <mxnet/base.h>
#include <dmlc/logging.h>
#include <mshadow/tensor.h>
#include <utility>
#include <vector>
#include <string>
#include "./inst_vector.h"

namespace mxnet {
namespace io {
// Batch parameters
struct BatchParam : public dmlc::Parameter<BatchParam> {
  /*! \brief label width */
  index_t batch_size;
  /*! \brief input shape */
  TShape input_shape;
  /*! \brief label width */
  index_t label_width;
  /*! \brief use round roubin to handle overflow batch */
  bool round_batch;
  /*! \brief skip read */
  bool test_skipread;
  /*! \brief silent */
  bool silent;
  // declare parameters
  DMLC_DECLARE_PARAMETER(BatchParam) {
    DMLC_DECLARE_FIELD(batch_size)
        .describe("Batch size.");
    index_t input_shape_default[] = {3, 224, 224};
    DMLC_DECLARE_FIELD(input_shape)
        .set_default(TShape(input_shape_default, input_shape_default + 3))
        .set_expect_ndim(3).enforce_nonzero()
        .describe("Input shape of the neural net");
    DMLC_DECLARE_FIELD(label_width).set_default(1)
        .describe("Label width.");
    DMLC_DECLARE_FIELD(round_batch).set_default(true)
        .describe("Use round robin to handle overflow batch.");
    DMLC_DECLARE_FIELD(test_skipread).set_default(false)
        .describe("Skip read for testing.");
    DMLC_DECLARE_FIELD(silent).set_default(false)
        .describe("Whether to print batch information.");
  }
};

/*! \brief create a batch iterator from single instance iterator */
class BatchLoader : public IIterator<TBlobBatch> {
 public:
  explicit BatchLoader(IIterator<DataInst> *base):
      base_(base), head_(1), num_overflow_(0) {}
  virtual ~BatchLoader(void) {
    delete base_;
    // Free space for TblobBatch
    mshadow::FreeSpace(&data_holder_);
    mshadow::FreeSpace(&label_holder_);
  }
  inline void Init(const std::vector<std::pair<std::string, std::string> >& kwargs) {
    std::vector<std::pair<std::string, std::string> > kwargs_left;
    // init batch param, it could have similar param with
    kwargs_left = param_.InitAllowUnknown(kwargs);
    // init base iterator
    base_->Init(kwargs);
    std::vector<size_t> data_shape_vec;
    data_shape_vec.push_back(param_.batch_size);
    for (size_t shape_dim = 0; shape_dim < param_.input_shape.ndim(); shape_dim++)
        data_shape_vec.push_back(param_.input_shape[shape_dim]);
    data_shape_ = TShape(data_shape_vec.begin(), data_shape_vec.end());
    std::vector<size_t> label_shape_vec;
    label_shape_vec.push_back(param_.batch_size);
    label_shape_vec.push_back(param_.label_width);
    label_shape_ = TShape(label_shape_vec.begin(), label_shape_vec.end());
    // Init space for out_
    out_.inst_index = new unsigned[param_.batch_size];
    out_.data.clear();
    data_holder_ =  mshadow::NewTensor<mshadow::cpu>(data_shape_.get<4>(), 0.0f);
    label_holder_ =  mshadow::NewTensor<mshadow::cpu>(label_shape_.get<2>(), 0.0f);
    out_.data.push_back(TBlob(data_holder_));
    out_.data.push_back(TBlob(label_holder_));
  }
  inline void BeforeFirst(void) {
    if (param_.round_batch == 0 || num_overflow_ == 0) {
      // otherise, we already called before first
      base_->BeforeFirst();
    } else {
      num_overflow_ = 0;
    }
    head_ = 1;
  }
  inline bool Next(void) {
    out_.num_batch_padd = 0;

    // skip read if in head version
    if (param_.test_skipread != 0 && head_ == 0)
        return true;
    else
        this->head_ = 0;

    // if overflow from previous round, directly return false, until before first is called
    if (num_overflow_ != 0) return false;
    index_t top = 0;

    while (base_->Next()) {
      const DataInst& d = base_->Value();
      out_.inst_index[top] = d.index;
      mshadow::Copy(out_.data[1].get<mshadow::cpu, 2, float>()[top],
              d.data[1].get<mshadow::cpu, 1, float>());
      mshadow::Copy(out_.data[0].get<mshadow::cpu, 4, float>()[top],
              d.data[0].get<mshadow::cpu, 3, float>());
      if (++ top >= param_.batch_size) {
          return true;
      }
    }
    if (top != 0) {
      if (param_.round_batch != 0) {
        num_overflow_ = 0;
        base_->BeforeFirst();
        for (; top < param_.batch_size; ++top, ++num_overflow_) {
          CHECK(base_->Next()) << "number of input must be bigger than batch size";
          const DataInst& d = base_->Value();
          out_.inst_index[top] = d.index;
          mshadow::Copy(out_.data[1].get<mshadow::cpu, 2, float>()[top],
                  d.data[1].get<mshadow::cpu, 1, float>());
          mshadow::Copy(out_.data[0].get<mshadow::cpu, 4, float>()[top],
                  d.data[0].get<mshadow::cpu, 3, float>());
        }
        out_.num_batch_padd = num_overflow_;
      } else {
        out_.num_batch_padd = param_.batch_size - top;
      }
      return true;
    }
    return false;
  }
  virtual const TBlobBatch &Value(void) const {
    return out_;
  }

 private:
  /*! \brief batch parameters */
  BatchParam param_;
  /*! \brief output data */
  TBlobBatch out_;
  /*! \brief base iterator */
  IIterator<DataInst> *base_;
  /*! \brief on first */
  int head_;
  /*! \brief number of overflow instances that readed in round_batch mode */
  int num_overflow_;
  /*! \brief data shape */
  TShape data_shape_;
  /*! \brief label shape */
  TShape label_shape_;
  /*! \brief tensor to hold data */
  mshadow::Tensor<mshadow::cpu, 4, real_t> data_holder_;
  /*! \brief tensor to hold label */
  mshadow::Tensor<mshadow::cpu, 2, real_t> label_holder_;
};  // class BatchLoader
}  // namespace io
}  // namespace mxnet
#endif  // MXNET_IO_ITER_BATCHLOADER_H_
