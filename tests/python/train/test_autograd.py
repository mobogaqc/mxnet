# pylint: skip-file
from __future__ import print_function

import mxnet as mx
from mxnet import foo
from mxnet.foo import nn
import numpy as np
import logging
from common import get_data
from mxnet.contrib import autograd as ag
logging.basicConfig(level=logging.DEBUG)

# define network

def get_net():
    net = nn.Sequential()
    net.add(nn.Dense(128, activation='relu', prefix='fc1_'))
    net.add(nn.Dense(64, activation='relu', prefix='fc2_'))
    net.add(nn.Dense(10, prefix='fc3_'))
    return net

get_data.GetMNIST_ubyte()

batch_size = 100
train_data = mx.io.MNISTIter(
        image="data/train-images-idx3-ubyte",
        label="data/train-labels-idx1-ubyte",
        data_shape=(784,),
        label_name='sm_label',
        batch_size=batch_size, shuffle=True, flat=True, silent=False, seed=10)
val_data = mx.io.MNISTIter(
        image="data/t10k-images-idx3-ubyte",
        label="data/t10k-labels-idx1-ubyte",
        data_shape=(784,),
        label_name='sm_label',
        batch_size=batch_size, shuffle=True, flat=True, silent=False)

def score(net, ctx):
    metric = mx.metric.Accuracy()
    val_data.reset()
    for batch in val_data:
        data = foo.utils.load_data(batch.data[0], ctx_list=ctx, batch_axis=0)
        label = foo.utils.load_data(batch.label[0], ctx_list=ctx, batch_axis=0)
        outputs = []
        for x in data:
            outputs.append(net(x))
        metric.update(label, outputs)
    return metric.get()[1]

def train(net, epoch, ctx):
    net.all_params().initialize(mx.init.Xavier(magnitude=2.24), ctx=ctx)
    trainer = foo.Trainer(net.all_params(), 'sgd', {'learning_rate': 0.5})
    metric = mx.metric.Accuracy()

    for i in range(epoch):
        train_data.reset()
        for batch in train_data:
            data = foo.utils.load_data(batch.data[0], ctx_list=ctx, batch_axis=0)
            label = foo.utils.load_data(batch.label[0], ctx_list=ctx, batch_axis=0)
            outputs = []
            with ag.record():
                for x, y in zip(data, label):
                    z = net(x)
                    loss = foo.loss.softmax_cross_entropy_loss(z, y)
                    ag.compute_gradient([loss])
                    outputs.append(z)
            metric.update(label, outputs)
            trainer.step(batch.data[0].shape[0])
        name, acc = metric.get()
        metric.reset()
        print('training acc at epoch %d: %s=%f'%(i, name, acc))


def test_autograd():
    net1 = get_net()
    train(net1, 5, [mx.cpu(0), mx.cpu(1)])
    acc1 = score(net1, [mx.cpu(0)])
    acc2 = score(net1, [mx.cpu(0), mx.cpu(1)])
    assert acc1 > 0.95
    assert abs(acc1 - acc2) < 0.01
    net1.all_params().save('mnist.params')

    net2 = get_net()
    net2.all_params().load('mnist.params', ctx=[mx.cpu(0)])
    acc3 = score(net2, [mx.cpu(0)])
    assert abs(acc3 - acc1) < 0.0001


if __name__ == '__main__':
    test_autograd()
