# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Tx")
  file(MAKE_DIRECTORY "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Tx")
endif()
file(MAKE_DIRECTORY
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Tx/build/Tx"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Tx/build/_sysbuild/sysbuild/images/Tx-prefix"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Tx/build/_sysbuild/sysbuild/images/Tx-prefix/tmp"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Tx/build/_sysbuild/sysbuild/images/Tx-prefix/src/Tx-stamp"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Tx/build/_sysbuild/sysbuild/images/Tx-prefix/src"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Tx/build/_sysbuild/sysbuild/images/Tx-prefix/src/Tx-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Tx/build/_sysbuild/sysbuild/images/Tx-prefix/src/Tx-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Tx/build/_sysbuild/sysbuild/images/Tx-prefix/src/Tx-stamp${cfgdir}") # cfgdir has leading slash
endif()
