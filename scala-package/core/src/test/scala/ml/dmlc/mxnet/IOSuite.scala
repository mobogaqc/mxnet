package ml.dmlc.mxnet

import org.scalatest.{BeforeAndAfterAll, FunSuite}
import scala.sys.process._


class IOSuite extends FunSuite with BeforeAndAfterAll {
  test("test MNISTIter") {
    //get data
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
    val nBatch = 600
    var batchCount = 0
    //println("create MNISTIter")
    val mnist_iter = IO.createIterator("MNISTIter", params)
    //test_loop
    mnist_iter.reset()
    while(mnist_iter.iterNext()) {
      val batch = mnist_iter.next()
      batchCount+=1
    }
    assert(nBatch === batchCount)
    //test reset
    mnist_iter.reset()
    mnist_iter.iterNext()
    val label0 = mnist_iter.getLabel().toArray
    mnist_iter.iterNext()
    mnist_iter.iterNext()
    mnist_iter.iterNext()
    mnist_iter.reset()
    mnist_iter.iterNext()
    val label1 = mnist_iter.getLabel().toArray
    assert(label0 === label1)
  }

  /**
    * link error
    */
//  test("test ImageRecordIter") {
//    val params = Map(
//      "path_imgrec" -> "data/cifar/train.rec",
//      "mean_img" -> "data/cifar/cifar10_mean.bin",
//      "rand_crop" -> "false",
//      "and_mirror" -> "false",
//      "shuffle" -> "false",
//      "data_shape" -> "(3,28,28)",
//      "batch_size" -> "100",
//      "preprocess_threads" -> "4",
//      "prefetch_buffer" -> "1"
//    )
//    val img_iter = IO.createIterator("ImageRecordIter", params)
//    img_iter.reset()
//    while(img_iter.iterNext()) {
//
//    }
//  }
}
