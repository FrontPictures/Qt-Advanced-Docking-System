set(ads_SRCS
    ads_globals.cpp
    DockAreaTabBar.cpp
    DockAreaTitleBar.cpp
    DockAreaWidget.cpp
    DockContainerWidget.cpp
    DockGroupMenu.cpp
    DockManager.cpp
    DockOverlay.cpp
    DockSplitter.cpp
    DockWidget.cpp
    DockWidgetTab.cpp
    DockingStateReader.cpp
    DockFocusController.cpp
    ElidingLabel.cpp
    FloatingDockContainer.cpp
    FloatingDragPreview.cpp
    IconProvider.cpp
    DockComponentsFactory.cpp
    ads.qrc
    )

set(ads_HEADERS
    ads_globals.h
    DockAreaTabBar.h
    DockAreaTitleBar.h
    DockAreaTitleBar_p.h
    DockAreaWidget.h
    DockContainerWidget.h
    DockGroupMenu.h
    DockManager.h
    DockOverlay.h
    DockSplitter.h
    DockWidget.h
    DockWidgetTab.h
    DockingStateReader.h
    DockFocusController.h
    ElidingLabel.h
    FloatingDockContainer.h
    FloatingDragPreview.h
    IconProvider.h
    DockComponentsFactory.h
    )

if (UNIX)
    set(ads_SRCS linux/FloatingWidgetTitleBar.cpp ${ads_SRCS})
    set(ads_HEADERS linux/FloatingWidgetTitleBar.h ${ads_HEADERS})
endif()

target_sources(${PROJECT_NAME} PRIVATE ${ads_SRCS} ${ads_HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(${PROJECT_NAME} PRIVATE ADS_STATIC)
