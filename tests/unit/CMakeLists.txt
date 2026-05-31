# SPDX-License-Identifier: MIT

add_subdirectory_ifdef(CONFIG_ZPROJ test_zproj_eval)
add_subdirectory_ifdef(CONFIG_ZPROJ_BLOB_LOADER test_zproj_blob)
