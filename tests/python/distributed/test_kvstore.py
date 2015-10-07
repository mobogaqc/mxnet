#!/usr/bin/env python
# pylint: skip-file
#
# run on local machine
# $ ln -s ../../../dmlc-core/tracker/dmlc_local.py .
# $ ./dmlc_local.py -n 4 -s 4 ./test_kvstore.py

def updater(key, recved, stored):
    print "key: %d" & key
    stored += recved * 2

import mxnet as mx
import numpy as np
import time

def check_diff_to_scalar(A, x):
    """ assert A == x"""
    assert(np.sum(np.abs((A - x).asnumpy())) == 0), A.asnumpy()


# init
kv = mx.kv.create('dist')
my_rank = kv.get_rank()
nworker = kv.get_group_size()

rate = 2
shape = (2, 2)
keys = [3, 4, 5]

if my_rank == 0:
    # init key, value on servers
    kv.init(keys, [mx.nd.ones(shape)] * len(keys))
    # init updater on servers
    opt = mx.optimizer.create('test', rate)
    kv.set_optimizer(opt)

kv.barrier()
# print 'init worker %d' % my_rank

def test_sync_push_pull():
    val = mx.nd.zeros(shape)
    nrepeat = 2
    for i in range(nrepeat):
        kv.push(3, mx.nd.ones(shape)*(my_rank+1))
        kv.wait(3)
        kv.barrier()

    kv.pull(3, out = val)
    num = (nworker + 1 ) * nworker * rate / 2 * nrepeat + 1
    # print val.asnumpy()
    check_diff_to_scalar(val, num)

# kv.send_command_to_servers(1, 'hhh')

if __name__ == "__main__":
    test_sync_push_pull()

# kv.pull(3, out = val)
# print val.asnumpy()

# kv.push(keys, [mx.nd.ones(shape)*(my_rank+2)] * len(keys))
# # kv.push(keys, [mx.nd.ones(shape)*(my_rank+2)] * len(keys))
# kv.pull(3, out = val)
# print val.asnumpy()

# kv.push(keys, [mx.nd.ones(shape)*(my_rank+3)] * len(keys))
# kv.pull(3, out = val)
# print val.asnumpy()

# kv.push(keys, [mx.nd.ones(shape)*(my_rank+4)] * len(keys))
# kv.pull(3, out = val)
# print val.asnumpy()
