# SPDX-License-Identifier: MIT

add_subdirectory_ifdef(CONFIG_ZPROJ test_ARBITER_eval)
add_subdirectory_ifdef(CONFIG_ARBITER_BLOB_LOADER test_ARBITER_blob)
