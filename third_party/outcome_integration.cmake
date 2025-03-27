add_library(outcome_Integrated INTERFACE)
find_package(outcome CONFIG REQUIRED)
target_link_libraries(outcome_Integrated INTERFACE outcome::hl)
