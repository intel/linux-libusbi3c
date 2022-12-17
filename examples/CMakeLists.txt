#######################
# Samples configuration
#######################

# Include header files from the following paths
INCLUDE_DIRECTORIES(BEFORE ${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/src)

set(sample_files
  change_address.c
  configure_devices.c
  handle_hotjoins.c
  handle_in_band_interrupts.c
  manage_features.c
  read_data.c
  sample.c
  submit_ccc.c
  write_data.c
)

# build the sample files
foreach(sample_file ${sample_files})
  get_filename_component(filename ${sample_file} NAME_WE)
  add_executable(${filename} ${sample_file})
  target_link_libraries(${filename} usbi3c Threads::Threads)
endforeach()
