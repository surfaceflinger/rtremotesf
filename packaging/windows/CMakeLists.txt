# SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
#
# SPDX-License-Identifier: CC0-1.0

include("../CPackCommon.cmake")

set(CPACK_GENERATOR ZIP WIX)
set(CPACK_PACKAGE_NAME "Rtremotesf")
set(CPACK_PACKAGE_VENDOR "Rtremotesf")
set(CPACK_PACKAGE_EXECUTABLES "rtremotesf;rtremotesf")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Rtremotesf")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/license.rtf")
set(CPACK_WIX_VERSION 4)
set(CPACK_WIX_INSTALL_SCOPE perMachine)
set(CPACK_WIX_UPGRADE_GUID "6249d747-62e2-4721-b81e-6ea056ede2a2")
set(CPACK_WIX_PRODUCT_ICON "${TREMOTESF_WINDOWS_ICON}")
set(CPACK_WIX_EXTRA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/registry.wxs")
set(CPACK_WIX_PATCH_FILE "${CMAKE_CURRENT_SOURCE_DIR}/registry_feature_patch.wxs")

include(CPack)
