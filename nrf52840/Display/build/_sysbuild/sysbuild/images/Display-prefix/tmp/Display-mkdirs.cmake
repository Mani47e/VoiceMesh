# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/Display")
  file(MAKE_DIRECTORY "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/Display")
endif()
file(MAKE_DIRECTORY
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/Display/build/Display"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/Display/build/_sysbuild/sysbuild/images/Display-prefix"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/Display/build/_sysbuild/sysbuild/images/Display-prefix/tmp"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/Display/build/_sysbuild/sysbuild/images/Display-prefix/src/Display-stamp"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/Display/build/_sysbuild/sysbuild/images/Display-prefix/src"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/Display/build/_sysbuild/sysbuild/images/Display-prefix/src/Display-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/Display/build/_sysbuild/sysbuild/images/Display-prefix/src/Display-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/Display/build/_sysbuild/sysbuild/images/Display-prefix/src/Display-stamp${cfgdir}") # cfgdir has leading slash
endif()
