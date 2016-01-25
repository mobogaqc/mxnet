package ml.dmlc.mxnet

import org.scalatest.{BeforeAndAfterAll, FunSuite}
import scala.sys.process._


class IOSuite extends FunSuite with BeforeAndAfterAll {
  test("test MNISTIter") {
    // get data
    "./scripts/get_mnist_data.sh" !

    val params = Map(
      "image" -> "data/train-images-idx3-ubyte",
      "label" -> "data/train-labels-idx1-ubyte",
      "data_shape" -> "(784,)",
      "batch_size" -> "100",
      "shuffle" -> "1",
      "flat" -> "1",
      "silent" -> "0",
      "seed" -> "10"
    )

    val mnistIter = IO.MNISTIter(params)
    // test_loop
    mnistIter.reset()
    val nBatch = 600
    var batchCount = 0
    while(mnistIter.iterNext()) {
      val batch = mnistIter.next()
      batchCount+=1
    }
    // test loop
    assert(nBatch === batchCount)
    // test reset
    mnistIter.reset()
    mnistIter.iterNext()
    val label0 = mnistIter.getLabel().head.toArray
    val data0 = mnistIter.getData().head.toArray
    mnistIter.iterNext()
    mnistIter.iterNext()
    mnistIter.iterNext()
    mnistIter.reset()
    mnistIter.iterNext()
    val label1 = mnistIter.getLabel().head.toArray
    val data1 = mnistIter.getData().head.toArray
    assert(label0 === label1)
    assert(data0 === data1)
  }


  /**
    * disable this test for saving time
    */
//  test("test ImageRecordIter") {
//    //get data
//    "./scripts/get_cifar_data.sh" !
//
//    val params = Map(
//      "path_imgrec" -> "data/cifar/train.rec",
//      "mean_img" -> "data/cifar/cifar10_mean.bin",
//      "rand_crop" -> "False",
//      "and_mirror" -> "False",
//      "shuffle" -> "False",
//      "data_shape" -> "(3,28,28)",
//      "batch_size" -> "100",
//      "preprocess_threads" -> "4",
//      "prefetch_buffer" -> "1"
//    )
//    val imgIter = IO.ImageRecordIter(params);
//    imgIter.reset()
//    while(imgIter.iterNext()) {
//      val batch = imgIter.next()
//    }
//  }

//  test("test NDarryIter") {
//
//  }
}
