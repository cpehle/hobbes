add_library(hobbesevents
  events.C
  httpd.C
)

target_include_directories(hobbesevents
  PUBLIC
  ${HOBBES_MAIN_INCLUDE_DIR}/hobbes/events/  
  )
