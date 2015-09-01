# coding: utf-8
# pylint: disable=invalid-name, protected-access, too-many-locals, fixme
""" code for executor. """
from __future__ import absolute_import

import ctypes
from .base import _LIB
from .base import c_array, mx_uint, NArrayHandle, ExecutorHandle
from .base import check_call
from .narray import NArray

class Executor(object):
    """ Executor is the actual executing object of MXNet."""
    def __init__(self, handle):
        """Init an executor from handle

        Parameters
        ----------
        handle: ExecutorHandle
            ExecutorHandle generated by calling Bind
        """
        if not isinstance(handle, ExecutorHandle):
            raise TypeError("Handle type error")
        self.handle = handle

    def forward(self, is_train=True):
        """Do forward.

        Parameters
        ----------
        is_train: bool
            whether this forward is for evaluation purpose
            Note: for test only network, please indicate in Bind (TODO)
        """
        check_call(_LIB.MXExecutorForward(self.handle, is_train))

    def backward(self, grads):
        """Do backward on heads' gradient.

        Parameters
        ----------
        grads: Array of NArray
            heads' gradient
        """
        for obj in grads:
            if not isinstance(obj, NArray):
                raise TypeError("inputs must be NArray")
        narray = c_array(NArrayHandle, [item.handle for item in grads])
        check_call(_LIB.MXExecutorBackward(self.handle, len(grads), narray))

    def heads(self):
        """list all heads' output narray

        Returns
        -------
        A list of narray binded to the heads of executor.
        """
        # TODO: think of access, make heads read only.
        # (consider support read only NArray(NArrayView))
        # Otherwise some of the internal might depends on out_data
        # if user set the content of the head, the backward behavior can be incorrect.
        out_size = mx_uint()
        handles = ctypes.POINTER(NArrayHandle)()
        check_call(_LIB.MXExecutorHeads(self.handle, ctypes.byref(out_size), ctypes.byref(handles)))
        return [NArray(NArrayHandle(handles[i])) for i in range(out_size.value)]
