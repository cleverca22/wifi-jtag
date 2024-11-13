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
    cli.c
    custom_usb.c
    dap.c
    jtag.cpp
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
    pico_usb_reset_interface
    tinyusb_board
    tinyusb_device
    tinyusb_device_unmarked
  )
  target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include/)
else()
  add_executable(${PROJECT_NAME}
    arm_debug.cpp
    cli.c
    custom_usb.c
    dap.c
    jtag.cpp
    main.cpp
  )
  target_link_libraries(${PROJECT_NAME}
    hardware_pio
    pico_bootsel_via_double_reset
    pico_multicore
    pico_stdlib
    pico_unique_id
    pico_usb_reset_interface
    tinyusb_board
    tinyusb_device
    tinyusb_device_unmarked
  )
  target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include/)
endif()

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

add_compile_options(-DPICO_ENTER_USB_BOOT_ON_EXIT -DPICO_DEFAULT_UART_TX_PIN=4 -PICO_DEFAULT_UART_RX_PIN=5 -Duart_default=uart1)

pico_add_extra_outputs(${PROJECT_NAME})

pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 0)
