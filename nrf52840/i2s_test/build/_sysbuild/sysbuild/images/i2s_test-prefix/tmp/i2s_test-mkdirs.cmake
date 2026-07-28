# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/i2s_test")
  file(MAKE_DIRECTORY "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/i2s_test")
endif()
file(MAKE_DIRECTORY
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/i2s_test/build/i2s_test"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/i2s_test/build/_sysbuild/sysbuild/images/i2s_test-prefix"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/i2s_test/build/_sysbuild/sysbuild/images/i2s_test-prefix/tmp"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/i2s_test/build/_sysbuild/sysbuild/images/i2s_test-prefix/src/i2s_test-stamp"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/i2s_test/build/_sysbuild/sysbuild/images/i2s_test-prefix/src"
  "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/i2s_test/build/_sysbuild/sysbuild/images/i2s_test-prefix/src/i2s_test-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/i2s_test/build/_sysbuild/sysbuild/images/i2s_test-prefix/src/i2s_test-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/i2s_test/build/_sysbuild/sysbuild/images/i2s_test-prefix/src/i2s_test-stamp${cfgdir}") # cfgdir has leading slash
endif()
