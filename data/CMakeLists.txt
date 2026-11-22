# SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
#
# SPDX-License-Identifier: CC0-1.0

target_sources(tremotesf_objects PRIVATE icons/status/status.qrc icons/turtle.qrc)

function(download_with_retries url path expected_hash)
    get_filename_component(download_directory "${path}" DIRECTORY)
    file(MAKE_DIRECTORY "${download_directory}")
    foreach(attempt RANGE 1 3)
        message(STATUS "Downloading ${url} to ${path} (attempt ${attempt})")
        file(
            DOWNLOAD "${url}" "${path}"
            SHOW_PROGRESS
            TLS_VERIFY ON
            EXPECTED_HASH "${expected_hash}"
            STATUS download_status
        )
        list(GET download_status 0 download_status)
        if (download_status EQUAL 0)
            return()
        endif()
        file(REMOVE "${path}")
    endforeach()
    message(FATAL_ERROR "Download failed: ${url}")
endfunction()

if (TREMOTESF_UNIX_FREEDESKTOP)
    find_package(Gettext 0.22.5 REQUIRED)

    install(DIRECTORY "icons/hicolor" DESTINATION "${CMAKE_INSTALL_DATADIR}/icons" PATTERN ".DS_Store" EXCLUDE)

    set(po_dir "${CMAKE_CURRENT_SOURCE_DIR}/po")

    set(desktop_file_template "${CMAKE_CURRENT_SOURCE_DIR}/org.equeim.Rtremotesf.desktop.in")
    set(desktop_file_path "${CMAKE_CURRENT_BINARY_DIR}/org.equeim.Rtremotesf.desktop")
    add_custom_command(
        OUTPUT "${desktop_file_path}"
        COMMAND "${GETTEXT_MSGFMT_EXECUTABLE}"
        ARGS --desktop -d "${po_dir}" --template "${desktop_file_template}" -o "${desktop_file_path}"
        DEPENDS "${desktop_file_template}"
        VERBATIM
    )
    add_custom_target(desktop_file ALL DEPENDS "${desktop_file_path}")
    install(FILES "${desktop_file_path}" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

    set(appdata_template "${CMAKE_CURRENT_SOURCE_DIR}/org.equeim.Rtremotesf.appdata.xml.in")
    set(appdata_path "${CMAKE_CURRENT_BINARY_DIR}/org.equeim.Rtremotesf.appdata.xml")
    add_custom_command(
        OUTPUT "${appdata_path}"
        COMMAND "${GETTEXT_MSGFMT_EXECUTABLE}"
        ARGS --xml -d "${po_dir}" --template "${appdata_template}" -o "${appdata_path}"
        DEPENDS "${appdata_template}"
        VERBATIM
    )
    add_custom_target(appdata ALL DEPENDS "${appdata_path}")
    install(FILES "${appdata_path}" DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
elseif (WIN32 OR APPLE)
    message(STATUS "Building for Windows or macOS, deploying icon themes")

    install(DIRECTORY "icons/hicolor" DESTINATION "${TREMOTESF_EXTERNAL_RESOURCES_PATH}/icons" PATTERN ".DS_Store" EXCLUDE)

    install(
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/icons/hicolor-index.theme"
        DESTINATION "${TREMOTESF_EXTERNAL_RESOURCES_PATH}/icons/hicolor"
        RENAME "index.theme"
    )

    set(breeze_download_path "${CMAKE_CURRENT_BINARY_DIR}/breeze.tar.xz")
    set(breeze_version "6.31.0")
    string(REGEX REPLACE "^(.*)\\..*" "\\1" breeze_version_without_patch "${breeze_version}")
    if (NOT EXISTS "${breeze_download_path}")
        set(breeze_url "https://download.kde.org/stable/frameworks/${breeze_version_without_patch}/breeze-icons-${breeze_version}.tar.xz")
        download_with_retries(
            "${breeze_url}"
            "${breeze_download_path}"
            "SHA256=82ab1c13f782cb37176e7c13c5fb2c77536a65f24b85d09127f3575f917fb56d"
        )
        message(STATUS "Extracting ${breeze_download_path} to ${CMAKE_CURRENT_BINARY_DIR}")
        file(ARCHIVE_EXTRACT INPUT "${breeze_download_path}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
    endif()
    set(breeze_extracted_path "${CMAKE_CURRENT_BINARY_DIR}/breeze-icons-${breeze_version}")
    install(CODE "set(breeze_extracted_path [=[${breeze_extracted_path}]=])")
    install(CODE "set(TREMOTESF_EXTERNAL_RESOURCES_PATH [=[${TREMOTESF_EXTERNAL_RESOURCES_PATH}]=])")
    install(SCRIPT InstallBreezeIcons.cmake)

    target_compile_definitions(tremotesf PRIVATE TREMOTESF_BUNDLED_ICON_THEME="breeze")
endif()

if (APPLE)
    install(FILES icons/macos/rtremotesf.icns DESTINATION "${TREMOTESF_EXTERNAL_RESOURCES_PATH}")
endif ()

