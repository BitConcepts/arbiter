# SPDX-License-Identifier: MIT

if(CONFIG_ZPROJ)
  zephyr_library()
  zephyr_library_sources(
    lib/zproj_engine.c
    lib/zproj_fact_store.c
    lib/zproj_eval.c
    lib/zproj_trace.c
  )

  zephyr_library_sources_ifdef(CONFIG_ZPROJ_BLOB_LOADER
    lib/zproj_blob.c
  )

  zephyr_library_sources_ifdef(CONFIG_ZPROJ_ACTION_DISPATCHER
    lib/zproj_action.c
  )

  zephyr_include_directories(include)

  add_subdirectory_ifdef(CONFIG_ZPROJ subsys/zproj)
endif()
