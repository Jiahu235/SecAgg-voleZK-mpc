find_package(SEAL 3.6.4 REQUIRED)
include_directories(${SEAL_INCLUDE_DIRS})

ADD_SUBDIRECTORY(LinearLayer)

find_package(OpenMP REQUIRED)
find_package(Eigen3 REQUIRED)
add_library(FLBase
    fl_utils.cpp
)

target_link_libraries(FLBase
    PUBLIC
    ${EMP-OT_LIBRARIES}
    SEAL::seal
	# OpenMP::OpenMP_CXX
    # Eigen3::Eigen
	LinearLayer
	emp-zk
)

#Testing macro
macro (add_test_executable_with_lib _name libs)
	add_executable(test_${_name} "${_name}.cpp")
	target_link_libraries(test_${_name} ${EMP-OT_LIBRARIES} SEAL::seal LinearLayer emp-zk FLBase)
endmacro()

macro (add_test_case _name)
	add_test_executable_with_lib(${_name} "")
  	add_test(NAME ${_name} COMMAND "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_${_name}" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/")
endmacro()

macro (add_test_case_with_run _name)
	add_test_executable_with_lib(${_name} "")
	add_test(NAME ${_name} COMMAND "./run" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_${_name}" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/")
endmacro()

# Test cases
# add_test_case_with_run(msi_relu)
# add_test_case_with_run(msi_relu_final)
# add_test_case_with_run(msi_linearlayer)
# add_test_case_with_run(msi_convlayer)
# add_test_case_with_run(msi_microbenchmark)
# add_test_case_with_run(msi_relu_preprocess)
# add_test_case_with_run(msi_relu_integrate)
# add_test_case_with_run(msi_average)

add_test_case_with_run(fl_mult)
add_test_case_with_run(fl_comp)
add_test_case_with_run(fl_lncheck)
add_test_case_with_run(fl_l2check)
add_test_case_with_run(fl_normball)
add_test_case_with_run(fl_cossim)
add_test_case_with_run(fl_aggregation)
