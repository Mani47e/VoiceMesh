cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(esb_voice_node)

# Add all folders containing .h headers to include path
target_include_directories(app PRIVATE
    src
    src/transport
    src/audio
    src/codec
    src/ringbuffer
    src/app
)

# List all source files
target_sources(app PRIVATE
    src/main.c
    src/transport/transport.c
    src/audio/audio.c
    src/codec/codec.c
    src/ringbuffer/ringbuffer.c
    src/app/appstate.c
)