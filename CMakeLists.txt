cmake_minimum_required(VERSION 3.13)

add_sources_from_folders(sources
    .
    stylesheets
    EXTENSIONS_LIST .h .cpp .qrc .css
    )

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(${PROJECT_NAME} PUBLIC ADS_STATIC)
