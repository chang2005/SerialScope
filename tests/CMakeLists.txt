find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(test_core
    test_core.cpp
)
target_link_libraries(test_core PRIVATE serial_core Qt6::Test)

qt_add_executable(test_serial_config
    test_serial_config.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/SerialConfigPanel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/FlowLayout.cpp
)
target_link_libraries(test_serial_config PRIVATE serial_core Qt6::Widgets Qt6::Test)

qt_add_executable(test_flowlayout
    test_flowlayout.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/FlowLayout.cpp
)
target_link_libraries(test_flowlayout PRIVATE Qt6::Widgets Qt6::Test)
target_include_directories(test_flowlayout PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_test(NAME test_core COMMAND test_core)
add_test(NAME test_serial_config COMMAND test_serial_config)
add_test(NAME test_flowlayout COMMAND test_flowlayout)
set_tests_properties(test_serial_config test_flowlayout PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

if(WIN32)
    # 测试程序直接放在构建根目录；复制 Qt 运行时，避免 PATH 中其他软件的
    # 旧版 Qt DLL（例如 STM32CubeProgrammer 自带的一套）被优先加载。
    foreach(_test_target test_core test_serial_config test_flowlayout)
        add_custom_command(TARGET ${_test_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:${_test_target}>
                    $<TARGET_FILE_DIR:${_test_target}>
            COMMAND_EXPAND_LISTS)
    endforeach()

    get_filename_component(_qt_plugin_dir "${Qt6_DIR}/../../../plugins" ABSOLUTE)
    add_custom_command(TARGET test_serial_config POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:test_serial_config>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_qt_plugin_dir}/platforms/qoffscreen.dll"
                "${_qt_plugin_dir}/platforms/qwindows.dll"
                "$<TARGET_FILE_DIR:test_serial_config>/platforms/"
        COMMENT "Copying platform plugins for tests and build-tree app")
endif()
