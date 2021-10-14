set(application_srcs src/cctrl.cpp)

add_executable(cctrl.elf ${application_srcs})
target_link_libraries(
    cctrl.elf
    PRIVATE
        ${CMAKE_PROJECT_NAME}_warnings
        utl
        serial
        memman
)
target_compile_features(cctrl.elf PRIVATE cxx_std_20)

add_custom_target(
    cctrl.hex ALL DEPENDS cctrl.elf
    COMMAND ${CMAKE_OBJCOPY}
        -O ihex -R .eeprom cctrl.elf cctrl.hex
    COMMENT
        "Creating uploadable binary cctrl.hex"
)

set_target_properties(
    cctrl.hex
    PROPERTIES
        OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/cctrl.hex
)

add_custom_target(
    print_size ALL
    COMMAND
        avr-size --format=berkeley ${CMAKE_CURRENT_BINARY_DIR}/cctrl.elf
    DEPENDS
        cctrl.elf
        cctrl.hex
)