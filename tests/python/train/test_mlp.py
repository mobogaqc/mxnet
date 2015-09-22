# pylint: skip-file
import mxnet as mx
import numpy as np
import os, sys
import pickle as pickle
import logging
from common import get_data

# symbol net
batch_size = 100
data = mx.symbol.Variable('data')
fc1 = mx.symbol.FullyConnected(data, name='fc1', num_hidden=128)
act1 = mx.symbol.Activation(fc1, name='relu1', act_type="relu")
fc2 = mx.symbol.FullyConnected(act1, name = 'fc2', num_hidden = 64)
act2 = mx.symbol.Activation(fc2, name='relu2', act_type="relu")
fc3 = mx.symbol.FullyConnected(act2, name='fc3', num_hidden=10)
softmax = mx.symbol.Softmax(fc3, name = 'sm')

num_round = 4
prefix = './mlp'
model = mx.model.FeedForward(softmax,
                             [mx.cpu(i) for i in range(2)],
                             num_round=num_round,
                             learning_rate=0.01, wd=0.0004,
                             momentum=0.9)
#check data
get_data.GetMNIST_ubyte()

train_dataiter = mx.io.MNISTIter(
        image="data/train-images-idx3-ubyte",
        label="data/train-labels-idx1-ubyte",
        data_shape=(784,),
        batch_size=batch_size, shuffle=True, flat=True, silent=False, seed=10)
val_dataiter = mx.io.MNISTIter(
        image="data/t10k-images-idx3-ubyte",
        label="data/t10k-labels-idx1-ubyte",
        data_shape=(784,),
        batch_size=batch_size, shuffle=True, flat=True, silent=False)

def test_mlp():
    # print logging by default
    logging.basicConfig(level=logging.DEBUG)
    console = logging.StreamHandler()
    console.setLevel(logging.DEBUG)
    logging.getLogger('').addHandler(console)

    model.fit(X=train_dataiter,
              eval_data=val_dataiter,
              iter_end_callback=mx.model.do_checkpoint(prefix))
    logging.info('Finish fit...')
    prob = model.predict(val_dataiter)
    logging.info('Finish predict...')
    val_dataiter.reset()
    y = np.concatenate([label.asnumpy() for _, label in val_dataiter]).astype('int')
    py = np.argmax(prob, axis=1)
    acc1 = float(np.sum(py == y)) / len(y)
    logging.info('final accuracy = %f', acc1)
    assert(acc1 > 0.95)

    # pickle the model
    smodel = pickle.dumps(model)
    model2 = pickle.loads(smodel)
    prob2 = model2.predict(val_dataiter)
    assert np.sum(np.abs(prob - prob2)) == 0

    # load model from checkpoint
    model3 = mx.model.FeedForward.load(prefix, num_round)
    prob3 = model3.predict(val_dataiter)
    assert np.sum(np.abs(prob - prob3)) == 0

    # save model explicitly
    model.save(prefix, 128)
    model4 = mx.model.FeedForward.load(prefix, 128)
    prob4 = model4.predict(val_dataiter)
    assert np.sum(np.abs(prob - prob4)) == 0

    for i in range(num_round):
        os.remove('%s-%04d.params' % (prefix, i + 1))
    os.remove('%s-symbol.json' % prefix)
    os.remove('%s-0128.params' % prefix)


if __name__ == "__main__":
    test_mlp()
