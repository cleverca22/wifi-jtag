cmake_minimum_required(VERSION 3.12)
include(pico_sdk_import.cmake)

set(PICO_BOARD pico_w)
project(wifi-jtag C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()


add_compile_options(
  -Wall
  -DWIFI_SSID=\"${WIFI_SSID}\"
  -DWIFI_PASSWORD=\"${WIFI_PASSWORD}\"
)


include_directories(
  include
  ${CMAKE_CURRENT_LIST_DIR}
)

if (PICO_BOARD STREQUAL "pico_w")
  add_executable(${PROJECT_NAME}
    main.cpp
    server.cpp
    socket.cpp
    websocket.cpp
    #ws.cpp
    #rawdraw_websocket.c
  )
  target_link_libraries(${PROJECT_NAME}
    pico_cyw43_arch_lwip_threadsafe_background
    pico_multicore
    pico_stdlib
  )
else()
  add_executable(${PROJECT_NAME}
    main.cpp
  )
  target_link_libraries(${PROJECT_NAME}
    pico_multicore
    pico_stdlib
  )
endif()

add_compile_options(-DPICO_ENTER_USB_BOOT_ON_EXIT)

pico_add_extra_outputs(${PROJECT_NAME})

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)
