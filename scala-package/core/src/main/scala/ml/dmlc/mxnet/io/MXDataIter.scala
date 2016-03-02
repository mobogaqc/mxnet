package ml.dmlc.mxnet.io

import ml.dmlc.mxnet.Base._
import ml.dmlc.mxnet.{DataPack, DataBatch, DataIter, NDArray, Shape}
import ml.dmlc.mxnet.IO._
import org.slf4j.LoggerFactory

import scala.collection.mutable.ListBuffer

/**
 * DataIter built in MXNet.
 * @param handle the handle to the underlying C++ Data Iterator
 */
// scalastyle:off finalize
class MXDataIter private[mxnet](private[mxnet] val handle: DataIterHandle,
                                private val dataName: String = "data",
                                private val labelName: String = "label") extends DataIter {
  private val logger = LoggerFactory.getLogger(classOf[MXDataIter])

  // use currentBatch to implement hasNext
  // (may be this is not the best way to do this work,
  // fix me if any better way found)
  private var currentBatch: DataBatch = null
  iterNext()
  private val data = currentBatch.data(0)
  private val label = currentBatch.label(0)
  reset()

  // properties
  val _provideData: Map[String, Shape] = Map(dataName -> data.shape)
  val _provideLabel: Map[String, Shape] = Map(labelName -> label.shape)
  override val batchSize = data.shape(0)
  private var disposed = false

  override protected def finalize(): Unit = {
    dispose()
  }

  /**
   * Release the native memory.
   * The object shall never be used after it is disposed.
   */
  def dispose(): Unit = {
    if (!disposed) {
      _LIB.mxDataIterFree(handle)
      disposed = true
    }
  }

  /**
   * reset the iterator
   */
  override def reset(): Unit = {
    // TODO: self._debug_at_begin = True
    currentBatch = null
    checkCall(_LIB.mxDataIterBeforeFirst(handle))
  }

  override def next(): DataBatch = {
    // TODO
    // if self._debug_skip_load and not self._debug_at_begin:
    //   return DataBatch(data =[self.getdata()], label =[self.getlabel()],
    //                    pad = self.getpad(), index = self.getindex())
    if (currentBatch == null) {
      iterNext()
    }

    if (currentBatch != null) {
      val batch = currentBatch
      currentBatch = null
      batch
    } else {
      // TODO raise StopIteration
      null
    }
  }

  /**
   * Iterate to next batch
   * @return whether the move is successful
   */
  private def iterNext(): Boolean = {
    val next = new RefInt
    checkCall(_LIB.mxDataIterNext(handle, next))
    currentBatch = null
    if (next.value > 0) {
      currentBatch = new DataBatch(data = getData(), label = getLabel(),
        index = getIndex(), pad = getPad())
    }
    next.value > 0
  }

  /**
   * get data of current batch
   * @return the data of current batch
   */
  override def getData(): IndexedSeq[NDArray] = {
    val out = new NDArrayHandleRef
    checkCall(_LIB.mxDataIterGetData(handle, out))
    IndexedSeq(new NDArray(out.value, writable = false))
  }

  /**
   * Get label of current batch
   * @return the label of current batch
   */
  override def getLabel(): IndexedSeq[NDArray] = {
    val out = new NDArrayHandleRef
    checkCall(_LIB.mxDataIterGetLabel(handle, out))
    IndexedSeq(new NDArray(out.value, writable = false))
  }

  /**
   * Get the index of current batch
   * @return the index of current batch
   */
  override def getIndex(): IndexedSeq[Long] = {
    val outIndex = new ListBuffer[Long]
    val outSize = new RefLong
    checkCall(_LIB.mxDataIterGetIndex(handle, outIndex, outSize))
    outIndex.toIndexedSeq
  }

  /**
   * get the number of padding examples
   * in current batch
   * @return number of padding examples in current batch
   */
  override def getPad(): MXUint = {
    val out = new MXUintRef
    checkCall(_LIB.mxDataIterGetPadNum(handle, out))
    out.value
  }

  // The name and shape of data provided by this iterator
  override def provideData: Map[String, Shape] = _provideData

  // The name and shape of label provided by this iterator
  override def provideLabel: Map[String, Shape] = _provideLabel

  override def hasNext: Boolean = {
    if (currentBatch != null) {
      true
    } else {
      iterNext()
    }
  }
}

// scalastyle:on finalize
class MXDataPack(val iterName: String,
                 val params: Map[String, String]) extends DataPack {
  /**
    * get data iterator
    * @return DataIter
    */
  override def iterator: DataIter = {
    createIterator(iterName, params)
  }
}
