PROJECT NOT UNDER ACTIVE MANAGEMENT

This project will no longer be maintained by Intel.

Intel has ceased development and contributions including, but not limited to, maintenance, bug fixes, new releases, or updates, to this project.  

Intel no longer accepts patches to this project.

If you have an ongoing need to use this project, are interested in independently developing it, or would like to maintain patches for the open source software community, please create your own fork of this project.  

Contact: webadmin@linux.intel.com

# USB I3C* Library for Linux* OS

## About

The **USB I3C\* Library for Linux\* OS** is a C user-space library to communicate with I3C devices behind a USB I3C device. It is intended to be used by developers to facilitate the development of a host software stack to control and communicate with the USB I3C device.

It allows a host to connect to an I3C controller within a USB I3C device to configure and initialize an I3C bus and its target devices. It allows the host to send commands and write/read data to/from I3C or I2C target devices synchronously or asynchronously. And allows the host to handle In-band Interrupts sent by I3C target devices with user defined actions.

The library implements the operational model and data structures described in the [USB I3C Device Class specification](https://www.usb.org/document-library/usb-i3c-device-class-specification).

## Supported Platforms
 - Linux

## Building Dependencies

The following dependencies need to be installed to build the library:
 - cmake
 - libusb
 - doxygen (only if building the documentation)
 - cmocka (only if building the tests)

## Building The Library

Create a build directory:
`$ mkdir build`

Configure the build:
`$ cmake -DCMAKE_BUILD_TYPE=Release -B build/`

Optionally, you can also enable building the library's documentation and code samples:
`$ cmake -DDOCUMENTATION=ON -DBUILD_SAMPLES=ON -DCMAKE_BUILD_TYPE=Release -B build/`

Build the binary:
`$ cmake --build build/`

## Installing The Library

To install the library in the default location (*`/usr/local`*) you can run the install target:
`$ cmake --build build/ --target install`

You can install the library in a different location by specifying it using the *`CMAKE_INSTALL_PREFIX`* variable.

Likewise, uninstalling the binaries from the system can be done using the uninstall target:
`cmake --build build/ --target uninstall`

## Getting Started

With the library installed, all you need is to reference the usbi3c header in your source.

Please refer to the [code samples](./examples) to see examples of using the library.

For further information please build and read the library's documentation.

## How To Contribute
The **USB I3C\* Library for Linux\* OS** is looking for maintainers and welcomes contributors, we are more than willing to review your patches! For more information, take a look at the [CONTRIBUTING](./CONTRIBUTING.md) guide to see how you can help.

## Reporting Problems

Use the [GitHub issue tracker](https://github.com/intel/linux-libusbi3c/issues) to report bugs, issues and feature requests.
