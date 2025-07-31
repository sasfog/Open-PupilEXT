
# @author Gabor Benyei

# This thing should exist, but it does not ba default. On linux we can use the pkgconfig method,
#  but on windows we would need a Findglib.cmake to find the .lib files. Here it is

if(APPLE)

elseif(MSVC)

    # TODO: include paths

    set(glib_LIB_SEARCH_PATH "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/")

    find_library(glib_gio_LIBRARY
            NAMES gio-2.0.lib
            PATHS ${glib_LIB_SEARCH_PATH})
    find_library(glib_girepository_LIBRARY
            NAMES girepository-2.0.lib
            PATHS ${glib_LIB_SEARCH_PATH})
    find_library(glib_glib_LIBRARY
            NAMES glib-2.0.lib
            PATHS ${glib_LIB_SEARCH_PATH})
    find_library(glib_gmodule_LIBRARY
            NAMES gmodule-2.0.lib
            PATHS ${glib_LIB_SEARCH_PATH})
    find_library(glib_gobject_LIBRARY
            NAMES gobject-2.0.lib
            PATHS ${glib_LIB_SEARCH_PATH})
    find_library(glib_gthread_LIBRARY
            NAMES gthread-2.0.lib
            PATHS ${glib_LIB_SEARCH_PATH})

    list(APPEND glib_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include/glib-2.0")
    # One .h file is in this strange location, likely due to vcpkg, but yet we include from there as well
    list(APPEND glib_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/glib-2.0/include")

    message(STATUS "Glib (glib-2.0) libs location: ${glib_LIBRARIES}")

    list(APPEND glib_LIBRARIES ${glib_gio_LIBRARY})
    list(APPEND glib_LIBRARIES ${glib_girepository_LIBRARY})
    list(APPEND glib_LIBRARIES ${glib_glib_LIBRARY})
    list(APPEND glib_LIBRARIES ${glib_gmodule_LIBRARY})
    list(APPEND glib_LIBRARIES ${glib_gobject_LIBRARY})
    list(APPEND glib_LIBRARIES ${glib_gthread_LIBRARY})

    if(glib_INCLUDE_DIRS AND glib_LIBRARIES)
        set(glib_FOUND TRUE)
    endif()

elseif(UNIX)

endif()

#mark_as_advanced(
#        glib_INCLUDE_DIR
#        glib_INCLUDE_DIRS
#        glib_LIBRARIES
#        glib_CONFIG
#)