cmake_minimum_required( VERSION 3.16 )

project( glibdictclient VERSION 1.0.0 )

set( LIBRARY_NAME ${CMAKE_PROJECT_NAME} )
set( LIBRARY_VERSION ${CMAKE_PROJECT_VERSION} )

set( PROGRAM_CONFIGURE_FILE "config" )

if( CMAKE_HOST_WIN32 )
	set( LIBRARY_LINE_BREAKER "\\r\\n" )
else()
	set( LIBRARY_LINE_BREAKER "\\n" )
endif()

configure_file( config.h.in config.h )
include_directories( ${CMAKE_CURRENT_BINARY_DIR} )

add_subdirectory( src )
