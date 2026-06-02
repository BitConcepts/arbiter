# SPDX-License-Identifier: MIT

if(CONFIG_ARBITER)
  zephyr_library()
  zephyr_library_sources(
    ${CMAKE_CURRENT_LIST_DIR}/lib/arbiter_engine.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/arbiter_fact_store.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/arbiter_eval.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/arbiter_trace.c
  )

  zephyr_library_sources_ifdef(CONFIG_ARBITER_BLOB_LOADER
    ${CMAKE_CURRENT_LIST_DIR}/lib/arbiter_blob.c
  )

  zephyr_library_sources_ifdef(CONFIG_ARBITER_ACTION_DISPATCHER
    ${CMAKE_CURRENT_LIST_DIR}/lib/arbiter_action.c
  )

  zephyr_library_sources_ifdef(CONFIG_ARBITER_HW_ACCEL
    ${CMAKE_CURRENT_LIST_DIR}/lib/arbiter_accel.c
  )

  zephyr_library_sources_ifdef(CONFIG_ARBITER_EVENT_DRIVEN
    ${CMAKE_CURRENT_LIST_DIR}/lib/arbiter_event.c
  )

  zephyr_library_sources_ifdef(CONFIG_ARBITER_COMPOSE
    ${CMAKE_CURRENT_LIST_DIR}/lib/arbiter_compose.c
  )

  zephyr_library_sources_ifdef(CONFIG_ARBITER_STATE_MACHINE
    ${CMAKE_CURRENT_LIST_DIR}/lib/arbiter_state.c
  )

  zephyr_include_directories(${CMAKE_CURRENT_LIST_DIR}/include)

  add_subdirectory_ifdef(CONFIG_ARBITER ${CMAKE_CURRENT_LIST_DIR}/subsys/arbiter)
endif()
