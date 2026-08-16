find_package(PkgConfig)
find_package(X11 REQUIRED)
pkg_check_modules(IBUS REQUIRED ibus-1.0)
pkg_get_variable(IBUS_PKGDATADIR ibus-1.0 pkgdatadir)

add_subdirectory(config)

add_executable(ibus-engine-unikey
    main.cpp
    engine.cpp
)
target_link_libraries(ibus-engine-unikey
    ukengine
    unikey-config
    ${IBUS_LIBRARIES}
    ${X11_LIBRARIES}
)
target_include_directories(ibus-engine-unikey PUBLIC ${IBUS_INCLUDE_DIRS} ${X11_INCLUDE_DIR})

configure_file(unikey.xml.in unikey.xml)

install(TARGETS ibus-engine-unikey DESTINATION ${LIBEXECDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/unikey.xml DESTINATION "${IBUS_PKGDATADIR}/component")
