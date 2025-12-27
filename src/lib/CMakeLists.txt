cmake_minimum_required( VERSION 4.1 )

project( ${LIBRARY_NAME} LANGUAGES C )

find_package( PkgConfig REQUIRED )
pkg_check_modules( GLIB2 REQUIRED glib-2.0 )
pkg_check_modules( GIO2 REQUIRED gio-2.0 )

include( GNUInstallDirs )

add_library( ${PROJECT_NAME} SHARED )
target_compile_features( ${PROJECT_NAME} PRIVATE c_std_17 )
set_target_properties( ${PROJECT_NAME} PROPERTIES
	VERSION ${LIBRARY_VERSION}
)

target_sources( ${PROJECT_NAME}
	PRIVATE
		glibdictclient.c

	PUBLIC
		FILE_SET publicHeaders
		TYPE HEADERS
		FILES
			glibdictclient.h
)

target_include_directories( ${PROJECT_NAME}
	PRIVATE
		${GLIB2_INCLUDE_DIRS}
		${GIO2_INCLUDE_DIRS}
)

target_link_directories( ${PROJECT_NAME}
	PRIVATE
		${GLIB2_LIBRARY_DIRS}
		${GIO2_LIBRARY_DIRS}
)

target_link_libraries( ${PROJECT_NAME}
	PRIVATE
		${GLIB2_LIBRARIES}
		${GIO2_LIBRARIES}
)

install( TARGETS ${PROJECT_NAME}
	LIBRARY
	FILE_SET publicHeaders
)
