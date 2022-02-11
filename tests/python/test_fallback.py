from py4dgeo.fallback import *
from py4dgeo._py4dgeo import (
    cylinder_workingset_finder as cxx_cylinder_workingset_finder,
    no_uncertainty as cxx_no_uncertainty,
    radius_workingset_finder as cxx_radius_workingset_finder,
    standard_deviation_uncertainty as cxx_standard_deviation_uncertainty,
)
from py4dgeo.m3c2 import M3C2

from . import epochs, compare_algorithms

import pytest


@pytest.mark.parametrize(
    "uncertainty_callback",
    [
        (cxx_standard_deviation_uncertainty, standard_deviation_uncertainty),
        (cxx_no_uncertainty, no_uncertainty),
    ],
)
@pytest.mark.parametrize(
    "workingset_callback",
    [
        (cxx_radius_workingset_finder, radius_workingset_finder),
        (cxx_cylinder_workingset_finder, cylinder_workingset_finder),
    ],
)
def test_fallback_implementations(epochs, uncertainty_callback, workingset_callback):
    class CxxTestM3C2(M3C2):
        def callback_uncertainty_calculation(self):
            return uncertainty_callback[0]

        def callback_workingset_finder(self):
            return workingset_callback[0]

    class PythonTestM3C2(M3C2):
        def callback_uncertainty_calculation(self):
            return uncertainty_callback[1]

        def callback_workingset_finder(self):
            return workingset_callback[1]

    # Instantiate a fallback M3C2 instance
    pym3c2 = CxxTestM3C2(
        epochs=epochs,
        corepoints=epochs[0].cloud,
        cyl_radii=(3.0,),
        normal_radii=(2.0,),
        max_distance=6.0,
    )

    # And a regular C++ based one
    m3c2 = PythonTestM3C2(
        epochs=epochs,
        corepoints=epochs[0].cloud,
        cyl_radii=(3.0,),
        normal_radii=(2.0,),
        max_distance=6.0,
    )

    compare_algorithms(m3c2, pym3c2)


def test_python_fallback_m3c2(epochs):
    # Instantiate a fallback M3C2 instance
    pym3c2 = PythonFallbackM3C2(
        epochs=epochs, corepoints=epochs[0].cloud, cyl_radii=(3.0,), normal_radii=(2.0,)
    )

    # And a regular C++ based one
    m3c2 = M3C2(
        epochs=epochs, corepoints=epochs[0].cloud, cyl_radii=(3.0,), normal_radii=(2.0,)
    )

    compare_algorithms(m3c2, pym3c2)


def test_python_exception_in_callback(epochs):
    # Define a fault algorithm
    class ExcM3C2(M3C2):
        def callback_workingset_finder(self):
            def callback(*args):
                1 / 0

            return callback

    # Instantiate it
    m3c2 = ExcM3C2(
        epochs=epochs, corepoints=epochs[0].cloud, cyl_radii=(3.0,), normal_radii=(2.0,)
    )

    # Running it should throw the proper exception despite taking a detour
    # throw multi-threaded C++ code.
    with pytest.raises(ZeroDivisionError):
        m3c2.run()
