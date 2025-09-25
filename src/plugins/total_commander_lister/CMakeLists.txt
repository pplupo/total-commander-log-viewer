set(KLOGG_LISTER_PLUGIN_SOURCES
    src/lister_plugin.cpp
    src/lister_viewer_widget.cpp
)

set(KLOGG_LISTER_PLUGIN_HEADERS
    include/lister_viewer_widget.h
    include/lister_plugin_api.h
)

add_library(klogg_lister_plugin SHARED
    ${KLOGG_LISTER_PLUGIN_SOURCES}
    ${KLOGG_LISTER_PLUGIN_HEADERS}
)

set_target_properties(klogg_lister_plugin PROPERTIES
    AUTOMOC ON
    OUTPUT_NAME "klogg_lister"
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_include_directories(klogg_lister_plugin
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(klogg_lister_plugin
    PRIVATE
        UNICODE
        _UNICODE
)

target_link_libraries(klogg_lister_plugin
    PRIVATE
        project_options
        project_warnings
        klogg_ui
        klogg_settings
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Concurrent
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Xml
)

add_dependencies(ci_build klogg_lister_plugin)
