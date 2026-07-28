# Install script for directory: /opt/nordic/ncs/v3.4.0/zephyr

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/opt/nordic/ncs/toolchains/ccc010f809/opt/zephyr-sdk/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/zephyr/arch/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/zephyr/lib/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/zephyr/soc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/zephyr/boards/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/zephyr/subsys/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/zephyr/drivers/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/nrf/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/mcuboot/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/mbedtls/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/trusted-firmware-m/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/cjson/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/azure-sdk-for-c/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/cirrus-logic/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/openthread/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/nrf_wifi/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/memfault-firmware-sdk/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/hostap/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/canopennode/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/chre/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/cmsis/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/cmsis-dsp/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/cmsis-nn/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/cmsis_6/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/dhara/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/fatfs/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/hal_nordic/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/hal_st/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/hal_tdk/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/hal_wurthelektronik/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/liblc3/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/libmetal/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/libsbc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/littlefs/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/loramac-node/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/lvgl/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/mipi-sys-t/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/nanopb/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/open-amp/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/percepio/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/picolibc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/segger/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/uoscore-uedhoc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/zcbor/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/nrfxlib/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/nrf_hw_models/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/modules/connectedhomeip/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/zephyr/kernel/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/zephyr/cmake/flash/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/zephyr/cmake/usage/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/zephyr/cmake/reports/cmake_install.cmake")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/mani/Documents/GitHub/VoiceMesh/nrf52840/ESB_Audio/Rx/build/Rx/zephyr/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
