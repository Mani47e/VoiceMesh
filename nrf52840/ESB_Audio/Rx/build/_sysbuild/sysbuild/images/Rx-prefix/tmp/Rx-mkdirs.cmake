# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx")
  file(MAKE_DIRECTORY "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx")
endif()
file(MAKE_DIRECTORY
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/_sysbuild/sysbuild/images/Rx-prefix"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/_sysbuild/sysbuild/images/Rx-prefix/tmp"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/_sysbuild/sysbuild/images/Rx-prefix/src/Rx-stamp"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/_sysbuild/sysbuild/images/Rx-prefix/src"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/_sysbuild/sysbuild/images/Rx-prefix/src/Rx-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/_sysbuild/sysbuild/images/Rx-prefix/src/Rx-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/_sysbuild/sysbuild/images/Rx-prefix/src/Rx-stamp${cfgdir}") # cfgdir has leading slash
endif()
