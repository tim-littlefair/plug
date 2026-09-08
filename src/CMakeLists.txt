add_subdirectory(com)
add_subdirectory(ui)

add_library(plug-effects-enum effects_enum.cpp)
add_executable(plug Main.cpp)
target_link_libraries(plug
                        PRIVATE
                            plug-version
                            plug-ui
                            plug-mustang
                            plug-communication
                            plug-communication-usb
                            plug-libusb
                            plug-updater
                            plug-effects-enum
                            build-libs
                        )

install(TARGETS plug EXPORT plug-config DESTINATION ${CMAKE_INSTALL_BINDIR})
