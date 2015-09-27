# coding: utf-8
# pylint: disable=invalid-name, protected-access, too-many-locals
"""Symbolic Executor component of MXNet."""
from __future__ import absolute_import

import ctypes
from .base import _LIB
from .base import mx_uint, NDArrayHandle, ExecutorHandle
from .base import check_call, c_array, py_str
from .ndarray import NDArray

class Executor(object):
    """ Executor is the actual executing object of MXNet."""
    def __init__(self, handle, symbol):
        """Constructor, used Symbol.bind and Symbol.simple_bind instead.

        Parameters
        ----------
        handle: ExecutorHandle
            ExecutorHandle generated by calling Bind

        See Also
        --------
        Symbol.bind : to create executor
        """
        if not isinstance(handle, ExecutorHandle):
            raise TypeError("Handle type error")
        self.handle = handle
        self.arg_arrays = []
        self.grad_arrays = []
        self.aux_arrays = []
        self.outputs = self._get_outputs()
        self._symbol = symbol
        self._arg_dict = None
        self._grad_dict = None
        self._aux_dict = None

    @staticmethod
    def _get_dict(names, ndarrays):
        """Get the dictionary given name and ndarray pairs."""
        nset = set()
        for nm in names:
            if nm in nset:
                raise ValueError('Duplicate names detected, %s' % str(names))
            nset.add(nm)
        return dict(zip(names, ndarrays))

    def _get_outputs(self):
        """list all the output ndarray

        Returns
        -------
        A list of ndarray binded to the heads of executor.
        """
        out_size = mx_uint()
        handles = ctypes.POINTER(NDArrayHandle)()
        check_call(_LIB.MXExecutorOutputs(self.handle,
                                          ctypes.byref(out_size), ctypes.byref(handles)))
        return [NDArray(NDArrayHandle(handles[i])) for i in range(out_size.value)]

    def forward(self, is_train=False, **kwargs):
        """Calculate the outputs specified by the binded symbol.

        Parameters
        ----------
        is_train: bool, optional
            whether this forward is for evaluation purpose.

        **kwargs
            Additional specification of input arguments.

        Examples
        --------
        >>> # doing forward by specifying data
        >>> texec.forward(is_train=True, data=mydata)
        >>> # doing forward by not specifying things, but copy to the executor before hand
        >>> mydata.copyto(texec.arg_dict['data'])
        >>> texec.forward(is_train=True)
        """
        if len(kwargs) != 0:
            arg_dict = self.arg_dict
            for name, array in kwargs.items():
                if not isinstance(array, NDArray):
                    raise ValueError('only accept keyword argument of NDArrays')
                if name not in arg_dict:
                    raise TypeError('Unknown argument %s' % name)
                array.copyto(arg_dict[name])

        check_call(_LIB.MXExecutorForward(
            self.handle,
            ctypes.c_int(int(is_train))))

    def backward(self, out_grads=None):
        """Do backward pass to get the gradient of arguments.

        Parameters
        ----------
        out_grads : NDArray or list of NDArray, optional
            Gradient on the outputs to be propagated back.
            This parameter is only needed when bind is called
            on outputs that are not a loss function.
        """
        if out_grads is None:
            out_grads = []
        elif isinstance(out_grads, NDArray):
            out_grads = [out_grads]

        for obj in out_grads:
            if not isinstance(obj, NDArray):
                raise TypeError("inputs must be NDArray")
        ndarray = c_array(NDArrayHandle, [item.handle for item in out_grads])
        check_call(_LIB.MXExecutorBackward(
            self.handle,
            mx_uint(len(out_grads)),
            ndarray))

    @property
    def arg_dict(self):
        """Get dictionary representation of argument arrrays.

        Returns
        -------
        arg_dict : dict of str to NDArray
            The dictionary that maps name of arguments to NDArrays.

        Raises
        ------
        ValueError : if there are duplicated names in the arguments.
        """
        if self._arg_dict is None:
            self._arg_dict = Executor._get_dict(
                self._symbol.list_arguments(), self.arg_arrays)
        return self._arg_dict

    @property
    def aux_dict(self):
        """Get dictionary representation of auxiliary states arrays.

        Returns
        -------
        aux_dict : dict of str to NDArray
            The dictionary that maps name of auxiliary states to NDArrays.

        Raises
        ------
        ValueError : if there are duplicated names in the auxiliary states.
        """
        if self._aux_dict is None:
            self._aux_dict = Executor._get_dict(
                self._symbol.list_auxiliary_states(), self.aux_arrays)
        return self._aux_dict

    def copy_params_from(self, arg_params, aux_params=None, allow_extra_params=False):
        """Copy parameters from arg_params, aux_params into executor's internal array.

        Parameters
        ----------
        arg_params : dict of str to NDArray
            Parameters, dict of name to NDArray of arguments

        aux_params : dict of str to NDArray, optional
            Parameters, dict of name to NDArray of auxiliary states.

        allow_extra_params : boolean, optional
            Whether allow extra parameters that are not needed by symbol
            If this is True, no error will be thrown when arg_params or aux_params
            contain extra parameters that is not needed by the executor.

        Raises
        ------
        ValueError
            If there is additional parameters in the dict but allow_extra_params=False
        """
        for name, array in arg_params.items():
            if name in self.arg_dict:
                array.copyto(self.arg_dict[name])
            else:
                if not allow_extra_params:
                    raise ValueError('Find name \"%s\" that is not in the arguments' % name)
        if aux_params is None:
            aux_params = {}
        for name, array in aux_params.items():
            if name in self.aux_dict:
                array.copyto(self.aux_dict[name])
            else:
                if not allow_extra_params:
                    raise ValueError('Find name %s that is not in the auxiliary states' % name)

    def debug_str(self):
        """Get a debug string about internal execution plan.

        Returns
        -------
        debug_str : string
            Debug string of the executor.
        """
        debug_str = ctypes.c_char_p()
        check_call(_LIB.MXExecutorPrint(
            self.handle, ctypes.byref(debug_str)))
        return py_str(debug_str.value)
