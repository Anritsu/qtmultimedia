# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

# FindWMF
# ---------
#
# Try to locate the Windows Media Foundation library.
# If found, this will define the following variables:
#
# ``WMF_FOUND``
#     True if Windows Media Foundation is available
# ``WMF_LIBRARIES``
#     The Windows Media Foundation set of libraries
#
# If ``WMF_FOUND`` is TRUE, it will also define the following
# imported target:
#
# ``WMF::WMF``
#     The Windows Media Foundation library to link to

find_library(WMF_STRMIIDS_LIBRARY strmiids HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
find_library(WMF_AMSTRMID_LIBRARY amstrmid HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
find_library(WMF_DMOGUIDS_LIBRARY dmoguids HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
find_library(WMF_UUID_LIBRARY uuid HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
find_library(WMF_MSDMO_LIBRARY msdmo HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
find_library(WMF_OLE32_LIBRARY ole32 HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
find_library(WMF_OLEAUT32_LIBRARY oleaut32 HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
find_library(WMF_MF_LIBRARY mf HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
find_library(WMF_MFUUID_LIBRARY mfuuid HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
find_library(WMF_MFPLAT_LIBRARY mfplat HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
find_library(WMF_MFCORE_LIBRARY mfcore HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
find_library(WMF_PROPSYS_LIBRARY propsys HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})


set(WMF_LIBRARIES ${WMF_STRMIIDS_LIBRARY} ${WMF_AMSTRMID_LIBRARY} ${WMF_DMOGUIDS_LIBRARY} ${WMF_UUID_LIBRARY}
                  ${WMF_MSDMO_LIBRARY} ${WMF_OLE32_LIBRARY} ${WMF_OLEAUT32_LIBRARY} ${WMF_MF_LIBRARY}
                  ${WMF_MFUUID_LIBRARY} ${WMF_MFPLAT_LIBRARY} ${WMF_MFCORE_LIBRARY} ${WMF_PROPSYS_LIBRARY})
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WMF REQUIRED_VARS
                                  WMF_STRMIIDS_LIBRARY WMF_AMSTRMID_LIBRARY WMF_DMOGUIDS_LIBRARY WMF_UUID_LIBRARY
                                  WMF_MSDMO_LIBRARY WMF_OLE32_LIBRARY WMF_OLEAUT32_LIBRARY WMF_MF_LIBRARY
                                  WMF_MFUUID_LIBRARY WMF_MFPLAT_LIBRARY WMF_MFCORE_LIBRARY WMF_PROPSYS_LIBRARY)

if(WMF_FOUND AND NOT TARGET WMF::WMF)
    add_library(WMF::WMF INTERFACE IMPORTED)
    set_target_properties(WMF::WMF PROPERTIES
                          INTERFACE_LINK_LIBRARIES "${WMF_LIBRARIES}"
                          IMPORTED_LINK_INTERFACE_LANGUAGES C)
endif()

mark_as_advanced(WMF_LIBRARIES)
