package ml.dmlc.mxnet

import ml.dmlc.mxnet.Base._

import scala.collection.mutable.ArrayBuffer

object Executor {
  // Get the dictionary given name and ndarray pairs.
  private def getDict(names: Array[String], ndarrays: Array[NDArray]): Map[String, NDArray] = {
    require(names.toSet.size == names.length, "Duplicate names detected")
    (names zip ndarrays).toMap
  }

  /**
   * Get input slice from the input shape.
    Parameters
    ----------
   * batch_size : int
   *     The number of samples in a mini-batch.
   * work_load_list : list of float or int, optional
   *     The list of work load for different devices,
   *     in the same order as ctx
   * Returns
   * -------
   * slices : list of slice
   *     The split slices to get a specific slice.
   * Raises
   * ------
   * ValueError
   *     If there are two many splits such that some slice can be empty.
   */
  private def splitInputSlice[@specialized(Int, Float, Double) V]
                              (batchSize: Int, workLoadList: Array[V])
                              (implicit num: Numeric[V]): Array[(Int, Int)] = {
    val totalWorkLoad = workLoadList.sum.asInstanceOf[Float]
    val batchNumList = workLoadList.map(workLoad =>
      math.round(workLoad.asInstanceOf[Float] * batchSize / totalWorkLoad))
    val batchNumSum = batchNumList.sum
    if (batchNumSum < batchSize) {
      batchNumList(batchNumList.length-1) += batchSize - batchNumSum
    }

    val slices = ArrayBuffer.empty[(Int, Int)]
    var end = 0
    batchNumList.foreach(batchNum => {
      val begin = math.min(end, batchSize)
      end = math.min(begin + batchNum, batchSize)
      require(begin < end, "Too many slices such that some splits are empty")
      slices.append((begin, end))
    })
    slices.toArray
  }
}

/**
 * Symbolic Executor component of MXNet
 * @author Yizhi Liu
 *
 * Constructor: used Symbol.bind and Symbol.simple_bind instead.
 * @param handle ExecutorHandle generated by calling Bind
 * @param symbol
 * @see Symbol.bind : to create executor
 */
class Executor(val handle: ExecutorHandle, val symbol: Symbol) {
  var argArrays: Array[NDArray] = null
  var gradArrays: Array[NDArray] = null
  var auxArrays: Array[NDArray] = null
  var outputs: Array[NDArray] = getOutputs
  var _argDict: Map[String, NDArray] = null
  var _auxDict: Map[String, NDArray] = null
  /*
  self._grad_dict = None
  self._monitor_callback = None
  */

  override def finalize(): Unit = {
    checkCall(_LIB.mxExecutorFree(handle))
  }

  /**
   * list all the output ndarray
   * @return A list of ndarray binded to the heads of executor.
   */
  private def getOutputs: Array[NDArray] = {
    val ndHandles = ArrayBuffer[NDArrayHandle]()
    checkCall(_LIB.mxExecutorOutputs(handle, ndHandles))
    ndHandles.toArray.map(new NDArray(_))
  }

  /**
   * Calculate the outputs specified by the binded symbol.
   * @param isTrain whether this forward is for evaluation purpose.
   * @param kwargs Additional specification of input arguments.
   */
  def forward(isTrain: Boolean, kwargs: (String, NDArray)*): Unit = {
    kwargs.foreach { case (name, array) =>
      require(argDict.contains(name), s"Unknown argument $name")
      array.copyTo(argDict(name))
    }
    checkCall(_LIB.mxExecutorForward(handle, if (isTrain) 1 else 0))
  }

  def forward(): Unit = {
    forward(isTrain = false)
  }

  /**
   * Do backward pass to get the gradient of arguments.
   * @param outGrads
   *   Gradient on the outputs to be propagated back.
   *   This parameter is only needed when bind is called
   *   on outputs that are not a loss function.
   */
  def backward(outGrads: Array[NDArray]):Unit = {
    require(outGrads != null)
    val ndArrayPtrs = outGrads.map(_.handle.value)
    checkCall(_LIB.mxExecutorBackward(handle, outGrads.length, ndArrayPtrs))
  }

  def backward(outGrad: NDArray): Unit = {
    require(outGrad != null)
    backward(Array(outGrad))
  }

  def backward(): Unit = {
    backward(Array.empty[NDArray])
  }

  /**
   * Install callback.
   * callback  Takes a string and an NDArrayHandle.
   */
  def setMonitorCallback(callback: (String, NDArrayHandle) => Unit): Unit = ???

  /**
   * Get dictionary representation of argument arrrays.
   * @return The dictionary that maps name of arguments to NDArrays.
   * @throws IllegalArgumentException if there are duplicated names in the arguments.
   */
  def argDict: Map[String, NDArray] = {
    if (_argDict == null) {
      _argDict = Executor.getDict(symbol.listArguments(), argArrays)
    }
    _argDict
  }

  /**
   * Get dictionary representation of auxiliary states arrays.
   * @return The dictionary that maps name of auxiliary states to NDArrays.
   * @throws IllegalArgumentException if there are duplicated names in the auxiliary states.
   */
  def auxDict: Map[String, NDArray] = {
    if (_auxDict == null) {
      _auxDict = Executor.getDict(
      symbol.listAuxiliaryStates(), auxArrays)
    }
    _auxDict
  }

  /**
   * Copy parameters from arg_params, aux_params into executor's internal array.
   * @param argParams : dict of name to NDArray of arguments
   * @param auxParams : dict of name to NDArray of auxiliary states.
   * @param allowExtraParams
   *        Whether allow extra parameters that are not needed by symbol
   *        If this is True, no error will be thrown when arg_params or aux_params
   *        contain extra parameters that is not needed by the executor.
   * @throws IllegalArgumentException If there is additional parameters in the dict but allow_extra_params=False
   */
  def copyParamsFrom(argParams: Map[String, NDArray],
                     auxParams: Map[String, NDArray],
                     allowExtraParams: Boolean = false): Unit = {
    argParams.foreach { case (name, array) =>
      if (argDict.contains(name)) {
        array.copyTo(argDict(name))
      } else {
        require(allowExtraParams, s"Find name $name that is not in the arguments")
      }
    }
    if (auxParams != null) {
      auxParams.foreach { case (name, array) =>
        if (auxDict.contains(name)) {
          array.copyTo(auxDict(name))
        } else {
          require(allowExtraParams, s"Find name $name that is not in the auxiliary states")
        }
      }
    }
  }

  def copyParamsFrom(argParams: Map[String, NDArray], allowExtraParams: Boolean): Unit = {
    copyParamsFrom(argParams, null, allowExtraParams)
  }

  def copyParamsFrom(argParams: Map[String, NDArray]): Unit = {
    copyParamsFrom(argParams, allowExtraParams = false)
  }

  /**
   * Get a debug string about internal execution plan.
   * @return Debug string of the executor.
   */
  def debugStr: String = {
    val str = new RefString
    checkCall(_LIB.mxExecutorPrint(handle, str))
    str.value
  }
}
