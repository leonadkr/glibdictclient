cmake_minimum_required( VERSION 3.16 )

# add_subdirectory( lib )

project( ${PROGRAM_NAME} LANGUAGES C )

find_package( PkgConfig REQUIRED )
pkg_check_modules( GLIB2 REQUIRED glib-2.0 )
pkg_check_modules( GIO2 REQUIRED gio-2.0 )

include( GNUInstallDirs )

add_compile_options( "-Wall" "-pedantic" )

add_executable( ${PROJECT_NAME}
	main.c )

install( TARGETS ${PROJECT_NAME}
	RUNTIME )

target_include_directories( ${PROJECT_NAME}
	PRIVATE
	${GLIB2_INCLUDE_DIRS}
	${GIO2_INCLUDE_DIRS} )

target_link_directories( ${PROJECT_NAME}
	PRIVATE
	${GLIB2_LIBRARY_DIRS}
	${GIO2_LIBRARY_DIRS} )

target_link_libraries( ${PROJECT_NAME}
	PRIVATE
	${GLIB2_LIBRARIES}
	${GIO2_LIBRARIES}
	glibdictclient )
