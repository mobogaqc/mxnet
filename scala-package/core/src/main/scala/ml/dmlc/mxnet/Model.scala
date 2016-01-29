package ml.dmlc.mxnet

import ml.dmlc.mxnet.Base.Shape
import ml.dmlc.mxnet.optimizer.SGD
import org.slf4j.{Logger, LoggerFactory}

/**
 * Describe the model flow
 * @author Yizhi Liu
 */
class Model
object Model {
  private val logger = LoggerFactory.getLogger(classOf[Model])
  /**
   * Create kvstore
   * This function select and create a proper kvstore given the kvstore type
   * @param kvStore KVStore type
   * @param numDevice The number of devices
   * @param argParams Model parameter, dict of name to NDArray of net's weights.
   * @return Option of created [[KVStore]] and whether or not update weight on it
   */
  private[mxnet] def createKVStore(kvStore: String,
                                   numDevice: Int,
                                   argParams: Map[String, NDArray]): (Option[KVStore], Boolean) = {
    if (numDevice == 1 && !kvStore.contains("dist")) {
      // no need to use kv for single device and single machine
      (None, false)
    } else {
      var kvType = kvStore
      if (kvType == "local") {
        // automatically select a proper local
        val maxSize = argParams.values.map(_.shape.product).max
        kvType =
          if (maxSize < 1024 * 1024 * 16) {
            "local_update_cpu"
          } else {
            "local_allreduce_cpu"
          }
        logger.info(s"Auto - select kvstore type = $kvType")
      }
      (Option(KVStore.create(kvType)), !kvType.contains("local_allreduce"))
    }
  }

  /**
   * Create a kvStore (wrap it with Option, None if given kvStore == null)
   * @param kvStore KVStore
   * @return Option of created [[KVStore]] and whether or not update weight on it
   */
  private def createKVStore(kvStore: KVStore): (Option[KVStore], Boolean) = {
    (Option(kvStore), kvStore != null && !kvStore.`type`.contains("local_allreduce"))
  }

  // Initialize kvstore
  private def initializeKVStore(kvStore: KVStore,
                                paramArrays: Array[Array[NDArray]],
                                argParams: Map[String, NDArray],
                                paramNames: Array[String],
                                updateOnKVStore: Boolean): Unit = {
    require(paramArrays.length == paramNames.length)
    for (idx <- 0 until paramArrays.length) {
      val paramOnDevs = paramArrays(idx)
      kvStore.init(idx, argParams(paramNames(idx)))
      if (updateOnKVStore) {
        kvStore.pull(idx, paramOnDevs, -idx)
      }
    }
  }

  // Perform update of param_arrays from grad_arrays on kvstore
  private def updateParamsOnKVStore(paramArrays: Array[Array[NDArray]],
                                    gradArrays: Array[Array[NDArray]],
                                    kvStore: Option[KVStore]): Unit = {
    (paramArrays zip gradArrays).zipWithIndex.foreach { case ((argList, gradList), index) =>
      if (gradList != null) {
        // push gradient, priority is negative index
        kvStore.foreach(_.push(index, gradList, -index))
        // pull back the weights
        kvStore.foreach(_.pull(index, argList, -index))
      }
    }
  }

  // Perform update of param_arrays from grad_arrays not on kvstore
  private def updateParams(paramArrays: Array[Array[NDArray]],
                           gradArrays: Array[Array[NDArray]],
                           updater: MXKVStoreUpdater,
                           numDevice: Int,
                           kvStore: Option[KVStore] = None) {
    (paramArrays zip gradArrays).zipWithIndex.foreach { case ((argList, gradList), index) =>
      if (gradList != null) {
        kvStore.foreach(kv => {
          // push gradient, priority is negative index
          kv.push(index, gradList, -index)
          // pull back the sum gradients, to the same locations.
          kv.pull(index, gradList, -index)
        })
        (argList zip gradList).zipWithIndex.foreach { case ((w: NDArray, g: NDArray), k: Int) =>
          // faked an index here, to make optimizer create diff
          // state for the same index but on diff devs,
          // (copy from python package) TODO(mli) use a better solution latter
          updater.update(index * numDevice + k, g, w)
        }
      }
    }
  }

  /**
   * TODO
   * Internal training function on multiple devices.
   * This function will also work for single device as well.
   * @param symbol The network configuration
   * @param ctx The training devices.
   * @param argNames Name of all arguments of the network.
   * @param paramNames Name of all trainable parameters of the network.
   * @param auxNames Name of all auxiliary states of the network.
   * @param argParams Model parameter, dict of name to NDArray of net's weights.
   * @param auxParams Model parameter, dict of name to NDArray of net's auxiliary states.
   * @param beginEpoch The begining training epoch.
   * @param endEpoch The end training epoch.
   * @param epochSize Number of batches in a epoch.
   *                  In default, it is set to ceil(num_train_examples / batch_size)
   * @param optimizer The optimization algorithm
   * @param kvStore The KVStore
   * @param updateOnKVStore whether or not perform weight updating on kvstore
   * @param trainData Training data iterator.
   * @param evalData Validation data iterator.
   * @param evalMetric A evaluation function.
   * @param epochEndCallback A callback that is invoked at end of each epoch.
   *                         This can be used to checkpoint model each epoch.
   * @param batchEndCallback A callback that is invoked at end of each batch.
   *                         This can be used to measure speed,
   *                         get result from evaluation metric. etc.
   * @param logger When not specified, default logger will be used.
   * @param workLoadList The list of work load for different devices, in the same order as ctx
   * @param monitor Monitor outputs, weights, and gradients for debugging
   * @note This function will inplace update the NDArrays in argParams and auxStates.
   */
  // scalastyle:off parameterNum
  private[mxnet] def trainMultiDevice(symbol: Symbol, ctx: Array[Context],
                                      argNames: Seq[String], paramNames: Seq[String],
                                      auxNames: Seq[String], argParams: Map[String, NDArray],
                                      auxParams: Map[String, NDArray],
                                      beginEpoch: Int, endEpoch: Int, epochSize: Int,
                                      optimizer: Optimizer,
                                      kvStore: Option[KVStore], updateOnKVStore: Boolean,
                                      trainData: DataIter = null, evalData: DataIter = null,
                                      evalMetric: EvalMetric = null,
                                      epochEndCallback: EpochEndCallback = null,
                                      batchEndCallback: BatchEndCallback = null,
                                      logger: Logger = logger,
                                      workLoadList: Seq[Float] = Nil,
                                      monitor: Option[Monitor] = None): Unit = {
    val executorManager = new DataParallelExecutorManager(
        symbol = symbol,
        ctx = ctx,
        trainData = trainData,
        paramNames = paramNames,
        argNames = argNames,
        auxNames = auxNames,
        workLoadList = workLoadList,
        logger = logger)

    monitor.foreach(executorManager.installMonitor)
    executorManager.setParams(argParams, auxParams)

    // TODO: initialize kvstore when updateOnKVStore = true

    // Now start training
    for (epoch <- beginEpoch until endEpoch) {
      // Training phase
      val tic = System.currentTimeMillis
      evalMetric.reset()
      var nBatch = 0
      var epochDone = false
      // Iterate over training data.
      while (!epochDone) {
        var doReset = true
        // TODO: make DataIter implement Iterator
        var dataBatch = trainData.next()
        while (doReset && dataBatch != null) {
          executorManager.loadDataBatch(dataBatch)
          monitor.foreach(_.tic())
          executorManager.forward(isTrain = true)
          executorManager.backward()
          if (updateOnKVStore) {
            updateParamsOnKVStore(executorManager.paramArrays,
                                  executorManager.gradArrays,
                                  kvStore)
          } else {
            val updater = Optimizer.getUpdater(optimizer)
            updateParams(executorManager.paramArrays,
                         executorManager.gradArrays,
                         updater, ctx.length,
                         kvStore)
          }
          monitor.foreach(_.tocPrint())
          // evaluate at end, so out_cpu_array can lazy copy
          evalMetric.update(dataBatch.label, executorManager.cpuOutputArrays)

          nBatch += 1
          // TODO: batch callback (for print purpose)

          // this epoch is done possibly earlier
          if (epochSize != -1 && nBatch >= epochSize) {
            doReset = false
          }
          dataBatch = trainData.next()
        }
        if (doReset) {
          trainData.reset()
        }

        // this epoch is done
        epochDone = (epochSize == -1 || nBatch >= epochSize)
      }

      val (name, value) = evalMetric.get
      logger.info(s"Epoch[$epoch] Train-$name=$value")
      val toc = System.currentTimeMillis
      logger.info(s"Epoch[$epoch] Time cost=${toc - tic}")

      // TODO: evaluation on evalData, epoch_end_callback
    }
  }
  // scalastyle:on parameterNum
}

trait EpochEndCallback {
  def invoke(epoch: Int, symbol: Symbol,
             argParams: Map[String, NDArray],
             auxStates: Map[String, NDArray]): Unit
}

trait BatchEndCallback {
  def invoke(epoch: Int, nBatch: Int, evalMetric: EvalMetric)
}

/**
 * Model class of MXNet for training and predicting feedforward nets.
 * This class is designed for a single-data single output supervised network.
 * @param symbol The symbol configuration of computation network.
 * @param ctx The device context of training and prediction.
 *            To use multi GPU training, pass in a list of gpu contexts.
 * @param numEpoch Training parameter, number of training epochs(epochs).
 * @param epochSize Number of batches in a epoch. In default, it is set to
 *                  ceil(num_train_examples / batch_size)
 * @param optimizer Training parameter, name or optimizer object for training.
 * @param initializer Training parameter, the initialization scheme used.
 * @param batchSize The batch size of training data.
 * @param argParams Model parameter, dict of name to NDArray of net's weights.
 * @param auxParams Model parameter, dict of name to NDArray of net's auxiliary states.
 * @param allowExtraParams Whether allow extra parameters that are not needed by symbol
 *                         to be passed by aux_params and arg_params.
 *                         If this is True, no error will be thrown when aux_params and arg_params
 *                         contain extra parameters than needed.
 * @param beginEpoch The begining training epoch.
 * TODO: @param kwargs The additional keyword arguments passed to optimizer.
 */
class FeedForward(val symbol: Symbol, val ctx: Array[Context] = Array(new Context("cpu")),
                  val numEpoch: Int = -1, val epochSize: Int = -1,
                  val optimizer: String = "sgd",
                  val initializer: Initializer = new Uniform(0.01f),
                  val batchSize: Int = 128,
                  argParams: Map[String, NDArray] = null,
                  auxParams: Map[String, NDArray] = null,
                  allowExtraParams: Boolean = false,
                  val beginEpoch: Int = 0) {
  private val LOG: Logger = LoggerFactory.getLogger(classOf[FeedForward])
  // check if symbol contain duplicated names.
  Executor.checkArguments(symbol)

  // rematch parameters to delete useless ones
  private var _argParams =
    if (allowExtraParams) {
      if (argParams != null) {
        val argNames = symbol.listArguments().toSet
        argParams.filter { case (k, v) => argNames.contains(k) }
      } else {
        null
      }
    } else {
      argParams
    }
  private var _auxParams =
    if (allowExtraParams) {
      if (auxParams != null) {
        val auxNames = symbol.listAuxiliaryStates().toSet
        auxParams.filter { case (k, v) => auxNames.contains(k) }
      } else {
        null
      }
    } else {
      auxParams
    }

  // internal helper state
  var predExec: Executor = null

  // Initialize weight parameters and auxiliary states
  private def initParams(inputShapes: Map[String, Shape], overwrite: Boolean = false)
      : (Seq[String], Seq[String], Seq[String]) = {
    val (argShapes, _, auxShapes) = symbol.inferShape(inputShapes)
    val argNames = symbol.listArguments()
    val inputNames = inputShapes.keys
    val paramNames = argNames.toSet -- inputNames.toSet
    val auxNames = symbol.listAuxiliaryStates()

    val paramNameShapes = (argNames zip argShapes).filter { case (name, _) =>
      paramNames.contains(name)
    }
    val argParams = paramNameShapes.map { case (name, shape) =>
      (name, NDArray.zeros(shape))
    }.toMap
    val auxParams = (auxNames zip auxShapes).map { case (name, shape) =>
      (name, NDArray.zeros(shape))
    }.toMap

    for ((k, v) <- argParams) {
      if (_argParams != null && _argParams.contains(k) && (!overwrite)) {
        argParams(k).set(_argParams(k))
      } else {
        initializer(k, v)
      }
    }

    for ((k, v) <- auxParams) {
      if (_auxParams != null && _auxParams.contains(k) && (!overwrite)) {
        auxParams(k).set(_auxParams(k))
      } else {
        initializer(k, v)
      }
    }

    _argParams = argParams
    _auxParams = auxParams
    (argNames, paramNames.toSeq, auxNames)
  }

  // Initialize the predictor module for running prediction.
  private def initPredictor(inputShapes: Map[String, Shape]): Unit = {
    if (this.predExec == null) {
      val predExec = symbol.simpleBind(ctx(0), gradReq = "null", shapeDict = inputShapes)
      predExec.copyParamsFrom(_argParams, _auxParams)
      Executor.checkArguments(symbol)
      this.predExec = predExec
    }
  }

  // Initialize the iterator given input.
  private def initIter(X: NDArray, y: NDArray, isTrain: Boolean): DataIter = {
    require(y != null || !isTrain, "y must be specified")
    val label = if (y == null) NDArray.zeros(X.shape(0)) else y
    require(label.shape.length == 1, "Label must be 1D")
    require(X.shape(0) == label.shape(0), "The numbers of data points and labels not equal")
    if (isTrain) {
      new NDArrayIter(X, label, batchSize, shuffle = isTrain, lastBatchHandle = "roll_over")
    } else {
      new NDArrayIter(X, label, batchSize, shuffle = false)
    }
  }

  // Initialize the iterator given eval_data.
  private def initEvalIter(evalData: (NDArray, NDArray)): DataIter = {
    if (evalData == null) {
      null
    } else {
      initIter(evalData._1, evalData._2, isTrain = true)
    }
  }

  /**
   * TODO
   * Fit the model.
   * @param trainData Training data. If X is an DataIter, the name or, if not available,
   *                  position, of its outputs should match the corresponding variable
   *                  names defined in the symbolic graph.
   * @param evalData If eval_data is numpy.ndarray/list/NDArray pair,
   *                 it should be (valid_data, valid_label).
   * @param evalMetric The evaluation metric, name of evaluation metric.
   *                   Or a customize evaluation function that returns the statistics
   *                   based on minibatch.
   * @param epochEndCallback A callback that is invoked at end of each epoch.
   *                         This can be used to checkpoint model each epoch.
   * @param batchEndCallback A callback that is invoked at end of each batch
   *                         For print purpose
   * @param kvStoreType A string kvstore type:
   *                    'local' : multi-devices on a single machine, will automatically
   *                              choose one from 'local_update_cpu', 'local_allreduce_cpu', and
   *                              'local_allreduce_device'
   *                    'dist_sync' : multi-machines with BSP
   *                    'dist_async' : multi-machines with partical asynchronous
   *                    In default uses 'local', often no need to change for single machine.
   * @param logger When not specified, default logger will be used.
   * @param workLoadList The list of work load for different devices, in the same order as ctx
   */
  def fit(trainData: DataIter, evalData: DataIter, evalMetric: String = "acc",
          kvStoreType: String = "local", logger: Logger = LOG,
          workLoadList: Seq[Float] = null): Unit = {
    val (argNames, paramNames, auxNames) =
      initParams(trainData.provideData ++ trainData.provideLabel)
    // TODO: kwargs.put("arg_names", argNames)

    // TODO: setup metric

    // create kvstore
    val (kvStore, updateOnKVStore) = Model.createKVStore(kvStoreType, ctx.length, _argParams)

    // init optmizer
    val batchSizeMultiplier = kvStore.map { kv =>
      if (kv.`type` == "dist_sync") {
        kv.numWorkers
      } else {
        1
      }
    }
    val batchSize = trainData.batchSize * batchSizeMultiplier.getOrElse(1)
    // TODO: temporarily hard-coded sgd optimizer, accuracy evalMetric
    val optimizer = new SGD(rescaleGrad = 1f / batchSize)
    Model.trainMultiDevice(
      symbol, ctx, argNames, paramNames, auxNames,
      _argParams, _auxParams,
      this.beginEpoch, this.numEpoch,
      this.epochSize,
      optimizer,
      kvStore, updateOnKVStore,
      trainData = trainData, evalData = evalData,
      evalMetric = new Accuracy(),
      logger = logger, workLoadList = workLoadList)
  }
}

object FeedForward {
  // Check if name is a data argument.
  private def isDataArg(name: String): Boolean = {
    name.endsWith("data") || name.endsWith("label")
  }
}
