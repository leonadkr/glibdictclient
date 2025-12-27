cmake_minimum_required( VERSION 4.1 )

project( glibdictclient VERSION 1.0.0 )

set( LIBRARY_NAME ${PROJECT_NAME} )
set( LIBRARY_VERSION ${PROJECT_VERSION} )

set( PROGRAM_NAME "glib-dict-client" )
set( PROGRAM_APP_ID "com.github.leonadkr.${PROGRAM_NAME}" )

if( CMAKE_HOST_WIN32 )
	set( LIBRARY_LINE_BREAKER "\\r\\n" )
else()
	set( LIBRARY_LINE_BREAKER "\\n" )
endif()

option( GLIBDICTCLIENT_UTIL "Build the console util." OFF )

configure_file( config.h.in config.h )
include_directories( ${CMAKE_CURRENT_BINARY_DIR} )

add_subdirectory( src/lib )

if( GLIBDICTCLIENT_UTIL )
	add_subdirectory( src )
endif()
