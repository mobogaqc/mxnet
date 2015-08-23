/*!
 *  Copyright (c) 2015 by Contributors
 * \file iter_mnist-inl.h
 * \brief iterator that takes mnist dataset
 */
#ifndef MXNET_IO_ITER_MNIST_INL_H_
#define MXNET_IO_ITER_MNIST_INL_H_
#include <mshadow/tensor.h>
#include <mxnet/io.h>
#include <mxnet/base.h>
#include <dmlc/io.h>
#include <dmlc/logging.h>
#include <dmlc/parameter.h>
#include <string>
#include <vector>
#include "../utils/random.h"

namespace mxnet {
namespace io {
// Define mnist io parameters
struct MNISTParam : public dmlc::Parameter<MNISTParam> {
  /*! \brief path */
  std::string path_img, path_label;
  /*! \brief whether to do shuffle */
  bool shuffle;
  /*! \brief whether to print info */
  bool silent;
  /*! \brief batch size */
  int batch_size;
  /*! \brief data mode */
  int input_flat;
  // declare parameters in header file
  DMLC_DECLARE_PARAMETER(Param) {
    DMLC_DECLARE_FIELD(path_img).set_default("./train-images-idx3-ubyte");
    DMLC_DECLARE_FIELD(path_label).set_default("./train-labels-idx1-ubyte");
    DMLC_DECLARE_FIELD(shuffle).set_default(false); 
    DMLC_DECLARE_FIELD(silent).set_default(false); 
    DMLC_DECLARE_FIELD(batch_size).set_default(128);
    DMLC_DECLARE_FIELD(input_flat).add_enum("flat", 1)
        .add_enum("noflat", 0).set_default(1);
  }
};
    
class MNISTIterator: public IIterator<DataBatch> {
 public:
  MNISTIterator(void) {
    img_.dptr_ = NULL;
    inst_offset_ = 0;
    rnd.Seed(kRandMagic);
    out_.data.resize(2);
  }
  virtual ~MNISTIterator(void) {
    if (img_.dptr_ != NULL) delete []img_.dptr_;
  }
  virtual void SetParam(const char *name, const char *val) {
    std::map<std::string, std::string> kwargs;
    kwargs[name] = val;
    param.Init(kwargs);
  }
  // intialize iterator loads data in
  virtual void Init(void) {
    this->LoadImage();
    this->LoadLabel();
    // set name
    this->SetDataName(std::string("data"));
    this->SetDataName(std::string("label"));
    if (param.input_flat == 1) {
      batch_data_.shape_ = mshadow::Shape4(param.batch_size, 1, 1, img_.size(1) * img_.size(2));
    } else {
      batch_data_.shape_ = mshadow::Shape4(param.batch_size, 1, img_.size(1), img_.size(2));
    }
    out_.inst_index = NULL;
    batch_label_.shape_ = mshadow::Shape2(param.batch_size, 1);
    batch_label_.stride_ = 1;
    batch_data_.stride_ = batch_data_.size(3);
    out_.batch_size = param.batch_size;
    if (param.shuffle) this->Shuffle();
    if (param.silent == 0) {
      mshadow::Shape<4> s = batch_data_.shape_;
      printf("MNISTIterator: load %u images, shuffle=%d, shape=%u,%u,%u,%u\n",
             (unsigned)img_.size(0), param.shuffle, s[0], s[1], s[2], s[3]);
    }
  }
  virtual void BeforeFirst(void) {
    this->loc_ = 0;
  }
  virtual bool Next(void) {
    if (loc_ + param.batch_size <= img_.size(0)) {
      batch_data_.dptr_ = img_[loc_].dptr_;
      batch_label_.dptr_ = &labels_[loc_];
      out_.data[0] = TBlob(batch_data_);
      out_.data[1] = TBlob(batch_label_);
      out_.inst_index = &inst_[loc_];
      loc_ += param.batch_size;
      return true;
    } else {
      return false;
    }
  }
  virtual const DataBatch &Value(void) const {
    return out_;
  }
  virtual void InitParams(const std::vector<std::pair<std::string, std::string> >& kwargs) {
    std::map<std::string, std::string> kmap(kwargs.begin(), kwargs.end());
    param.Init(kmap);
  }
 private:
  inline void LoadImage(void) {
    dmlc::Stream *stdimg = dmlc::Stream::Create(param.path_img.c_str(), "r");
    ReadInt(stdimg);
    int image_count = ReadInt(stdimg);
    int image_rows  = ReadInt(stdimg);
    int image_cols  = ReadInt(stdimg);

    img_.shape_ = mshadow::Shape3(image_count, image_rows, image_cols);
    img_.stride_ = img_.size(2);

    // allocate continuous memory
    img_.dptr_ = new float[img_.MSize()];
    for (int i = 0; i < image_count; ++i) {
      for (int j = 0; j < image_rows; ++j) {
        for (int k = 0; k < image_cols; ++k) {
          unsigned char ch;
          CHECK(stdimg->Read(&ch, sizeof(ch) != 0));
          img_[i][j][k] = ch;
        }
      }
    }
    // normalize to 0-1
    img_ *= 1.0f / 256.0f;
    delete stdimg;
  }
  inline void LoadLabel(void) {
    dmlc::Stream *stdlabel = dmlc::Stream::Create(param.path_label.c_str(), "r");
    ReadInt(stdlabel);
    int labels_count = ReadInt(stdlabel);
    labels_.resize(labels_count);
    for (int i = 0; i < labels_count; ++i) {
      unsigned char ch;
      CHECK(stdlabel->Read(&ch, sizeof(ch) != 0));
      labels_[i] = ch;
      inst_.push_back((unsigned)i + inst_offset_);
    }
    delete stdlabel;
  }
  inline void Shuffle(void) {
    rnd.Shuffle(&inst_);
    std::vector<float> tmplabel(labels_.size());
    mshadow::TensorContainer<cpu, 3> tmpimg(img_.shape_);
    for (size_t i = 0; i < inst_.size(); ++i) {
      unsigned ridx = inst_[i] - inst_offset_;
      mshadow::Copy(tmpimg[i], img_[ridx]);
      tmplabel[i] = labels_[ridx];
    }
    // copy back
    mshadow::Copy(img_, tmpimg);
    labels_ = tmplabel;
  }

 private:
  inline static int ReadInt(dmlc::Stream *fi) {
    unsigned char buf[4];
    CHECK(fi->Read(buf, sizeof(buf)) == sizeof(buf))
        << "invalid mnist format";
    return reinterpret_cast<int>(buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3]);
  }

 private:
  /*! \brief MNIST iter params */
  MNISTParam param;
  /*! \brief output */
  DataBatch out_;
  /*! \brief current location */
  index_t loc_;
  /*! \brief image content */
  mshadow::Tensor<cpu, 3> img_;
  /*! \brief label content */
  std::vector<float> labels_;
  /*! \brief batch data tensor */
  mshadow::Tensor<cpu, 4> batch_data_;
  /*! \brief batch label tensor  */
  mshadow::Tensor<cpu, 2> batch_label_;
  /*! \brief instance index offset */
  unsigned inst_offset_;
  /*! \brief instance index */
  std::vector<unsigned> inst_;
  // random sampler
  utils::RandomSampler rnd;
  // magic number to setup randomness
  static const int kRandMagic = 0;
};  // class MNISTIterator
}  // namespace io
}  // namespace mxnet
#endif  // MXNET_IO_ITER_MNIST_INL_H_
