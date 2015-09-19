from .ndarray import NDArray, zeros
def get_optimizer(name, **kwargs):
    """Optimizer factory

    Parameters
    ----------
    name: str
        Name of required optimizer
    kwargs: dict
        Parameters for optimizer

    Return
    ----------
    A required optimizer object

    """
    if name == "sgd" or name == "SGD":
        return SGD(**kwargs)
    else:
        raise Exception("Not implemented")


class SGD(object):
    """A very simple SGD optimizer with Nesterov method

    Parameters
    ----------
    learning_rate: float
        learning_rate value
    momentum: float
        momentum value
    weight_decay: float
        L2 regularization coefficient
    """
    def __init__(self, **kwargs):
        assert("learning_rate" in kwargs)
        assert("momentum" in kwargs)
        assert("weight_decay" in kwargs)
        self.lr = kwargs["learning_rate"]
        self.momentum = kwargs["momentum"]
        self.wd = kwargs["weight_decay"]
        self.batch_size = 0
        self.momentums = {}

    def update(self, weight, grad, states):
        assert(isinstance(weight, NDArray))
        assert(isinstance(grad, NDArray))
        if states not in self.momentums:
            self.momentums[states] = zeros(grad.shape, grad.context)
        mom = self.momentums[states]
        mom[:] *= self.momentum
        mom[:] += -self.lr * (grad / self.batch_size + self.wd * weight)
        weight[:] += mom

