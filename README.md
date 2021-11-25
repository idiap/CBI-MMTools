# CBI-MMTools

Copyright (c) 2021 Idiap Research Institute, http://www.idiap.ch/

Written by François Marelli <francois.marelli@idiap.ch>


## Description of Content
This repository contains plugins, device adapters and libraries for the operation of microscopy platforms using Micro-Manager, developed by the Computational BioImaging group at Idiap Research Institute.

* __buildscripts__: configuration files for the ant builds

* __compiled__: compiled artifacts

* __configurations__: device configurations for micromanager

* __DeviceAdapters__: device adapters for micromanager

    * __ArdIllu__ (François Marelli): control a Kohler illuminator using an Arduino board
    * __ArduControl__ (François Marelli): synchronization and modulation of light sources using an Arduino board

* __libraries__: libraries for micromanager

    * __OpticalRotation__ (François Marelli): algorithm that compensates the off-axis rotations to keep samples centered in the field of view

* __plugins__: plugins for micromanager

    * __StageControl4D__ (François Marelli): control GUI for a 4D stage with off-axis rotation compensation and calibration
    * __OpenLightControl__ (François Marelli): control GUI for the ArduControl modulation hardware

* __print__: 3D printing files for the platform

* __software__: additional software tools

## Install instructions


Compiled files are distributed under the `compiled` folder. You can also recompile them following the following instructions (pre-compiled unix device adapters are not distributed):

* *Building device adapters:*

  * Initialize the git submodules (either clone with `git clone -r ...` or run `git submodule init` in the cloned folder).

  * On Windows, open `DeviceAdapters/cbi_mm_devices.sln` and build the solution. You may eed to retarget the solution/projects:
    * right click on the solution, "retarget solution", upgrade all projects to your installed platform toolset
    * if this does not work, right click on the problematic projects, and in the General category of the Properties, 
        select the desired Platform toolset (it is likely you will have to do this for mmCoreAndDevices)

  * On Unix, run `autogen.sh` then `configure` (provide the same prefix as you used to install micro-manager), and finally run `make install` to build and install device adapters.

* *Building plugins/libraries:*

  * In the root folder, run `ant` to build all libraries and plugins. The generated jars will be located in `compiled/libraries` and `compiled/plugins`.
  * You can also run `ant clean` to remove all build files (it also removes compiled jars).

**Do not forget to copy the build results to the actual mm installation folder when you compile projects!**

* On Windows, the folders are the following:
  * DeviceAdapters: at the root of micro-manager install (usually under program files)
  * Libraries: under `plugins\Micro-Manager`
  * Plugins: under `mmplugins`

* On Unix, the folders are the following:
  * DeviceAdapters: `<prefix>/lib/micro-manager/` (this should be automatically installed by make install if you set your prefix right)
  * Libraries: `<prefix>/share/micro-manager/jars`
  * Plugins: `<prefix>/share/micro-manager/mmplugins`

## Adding new projects

* **DeviceAdapters**

In order to add new device adapters, it is easier to create the files in Microsoft Visual Studio, and then add the necessary tools for Unix builds.

  * Open the `DeviceAdapters/cbi_mm_devices.sln` (on Windows) and add a new project:
    1. Add a new project to the solution, select the dll template
    2. In the Property Manager panel (under the View menu), right click the new project and add the `DeviceAdapters/Common.props` sheet
    3. In the properties of the project (right click in solution explorer), set C/C++ -> Precompiled Headers -> Precompiled Header: Not Using Precompiled Header
    4. You may need to set in properties Advanced -> Target File Extension: .dll (if build process produces files with incorrect names)
    5. You may also need to add a project dependency with the `MMDevice-SharedRuntime` (`mmCoreAndDevices\MMDevice\MMDevice-SharedRuntime.vcxproj`), if it is not done automatically.
  * Copy the `Makefile.am` file from the ArdIllu device adapter to your device folder, and change all the occurences of `ArdIllu` to your device's name.
  * Add a line containing the name of your project in the `DeviceAdapters/Makefile.am`, *with trailing backslashes except for the last line*. It must come after the MMDevice line.
  * Add a line containing the name of your project in the `DeviceAdapters/configure.ac`. It must be after the MMDevice line (L78).

  If you follow these instructions, you should be able to compile your project following the instructions detailed above.

  * **Libraries/plugins**

  Create a new folder under `libraries` or `plugins` respectively. Copy the `build.xml` from `OpticalRotation` for a library, or from `StageControl4D` for a plugin. In that file, you only need to change the project name to match yours. *Make sure that your source files are located in a directory named `src/` directly at your project root.*

  By default, libraries are compiled before plugins and are automatically added as dependencies. If you need more complex inter-project dependencies (i.e. your library depends on an other library, or your plugin depends on an other plugin), use `subant` commands as in `AcquisitionScripting/build.xml` to override the `makedeps` target.


## Acknowledgements
    
This work was supported by the Swiss National Science Foundation (SNSF), grants:

* 206021_164022 *Platform for Reproducible Acquisition, Processing, and Sharing of Dynamic, Multi-Modal Data.* (PLATFORM_MMD)
* 200020_179217 *Computational biomicroscopy: advanced image processing methods to quantify live biological systems* (COMPBIO)