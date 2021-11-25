"""
This script generates the demo modulation files for OpenLightSync.

It can be used as a base to generate any modulation.
It requires numpy and matplotlib to be installed.
"""

# Copyright (c) 2021 Idiap Research Institute, http://www.idiap.ch/
# Written by François Marelli <francois.marelli@idiap.ch>
#
# This file is part of CBI-MMTools.
#
# CBI-MMTools is free software: you can redistribute it and/or modify
# it under the terms of the 3-Clause BSD License.
#
# CBI-MMTools is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# 3-Clause BSD License for more details.
#
# You should have received a copy of the 3-Clause BSD License along
# with CBI-MMTools. If not, see https://opensource.org/licenses/BSD-3-Clause.
#
# SPDX-License-Identifier: BSD-3-Clause

import json
import numpy as np
import matplotlib.pyplot as plt
import os


def save_modulations(file_name, n_steps, n_frames, digital, analog):
    ## Prepare the analog modulations as bytes
    analog = analog * 255
    analog = np.round(analog).astype(int)

    # Check that the format requirements are met
    d_values = set(digital)
    assert (d_values.issubset({0, 1}))
    assert (analog.min() >= 0)
    assert (analog.max() <= 255)
    assert (digital.size == n_steps * n_frames)
    assert (analog.size == n_steps * n_frames)

    # Convert arrays to strings
    digital = '-'.join(map(str, digital.astype(int)))
    analog = '-'.join(map(str, analog.astype(int)))

    # Generate the modulation dictionary
    modulation_dict = {
        'nframes': n_frames,
        'nsteps': n_steps,
        'digital': digital,
        'analog': analog
    }

    with open(file_name, 'w') as f:
        json.dump(modulation_dict, f)


def plot_modulations(n_steps, n_frames, digital, analog):
    ## Plot the modulation arrays
    fig, ax = plt.subplots(2, 1, sharex=True)
    ax[0].set_title('Modulation functions')

    # Create the full time axis
    t_plot = np.arange(n_frames * n_steps + 1) / N_STEPS
    d_plot = np.append(digital.copy(), digital[-1])
    a_plot = np.append(analog.copy(), analog[-1])
    ax[0].step(t_plot, d_plot, color='C1', where='post', label='digital')
    ax[0].set_ylabel('Digital')
    ax[1].step(t_plot, a_plot, color='C2', where='post', label='analog')
    ax[1].set_ylabel('Analog')

    ylim = (-0.05, 1.05)
    for axis in ax:
        axis.grid()
        axis.fill_between(t_plot,
                          ylim[0],
                          ylim[1],
                          where=(t_plot % 2 <= 1),
                          facecolor='grey',
                          alpha=0.2)
        axis.set_ylim(ylim)

    ax[-1].set_xlabel('Frame')
    fig.tight_layout()
    plt.show()


## Choose where to save the modulation file
file_path = os.path.dirname(os.path.abspath(__file__))
file_name = os.path.join(file_path, 'demo_modulation.json')

## Define the modulation parameters
N_FRAMES = 3
N_STEPS = 64

## Create the time array for one step
t_step = np.linspace(0, 1, N_STEPS, endpoint=False)

## Create the modulations for the first frame
# Digital: will be on all the time
d_step1 = np.ones_like(t_step)
# Analog: sine wave
a_step1 = (np.sin(t_step * 2 * np.pi) + 1) / 2

## Second frame
# Digital: a short centered pulse
pulse_width = 0.05
d_step2 = np.ones_like(t_step)
d_step2[t_step < (0.5 - pulse_width / 2)] = 0
d_step2[t_step >= (0.5 + pulse_width / 2)] = 0
# Analog: constant at max amplitude
a_step2 = np.ones_like(t_step)

## Third frame
# Digital: will be on all the time
d_step3 = np.ones_like(t_step)
# Analog: linear ramp
a_step3 = t_step

## Concatenate all steps to get the full modulation array
d_full = np.concatenate((d_step1, d_step2, d_step3))
a_full = np.concatenate((a_step1, a_step2, a_step3))

## Plot and save the modulations
plot_modulations(N_STEPS, N_FRAMES, d_full, a_full)
save_modulations(file_name, N_STEPS, N_FRAMES, d_full, a_full)
