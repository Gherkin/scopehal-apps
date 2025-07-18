add_executable(Filters
	main.cpp

	Filter_Add.cpp
	Filter_ACRMS.cpp
	Filter_DeEmbed.cpp
	Filter_EyePattern.cpp
	Filter_FIR.cpp
	Filter_FFT.cpp
	Filter_Subtract.cpp
	Filter_Upsample.cpp

	FrequencyMeasurement.cpp
)

include_directories(Filters
	${LIBFFTS_INCLUDE_DIRS})

target_link_libraries(Filters
	scopehal
	scopeprotocols
	Catch2::Catch2
	${LIBFFTS_LIBRARIES}
	)

#Needed because Windows does not support RPATH and will otherwise not be able to find DLLs when catch_discover_tests runs the executable
if(WIN32)
add_custom_command(TARGET Filters POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:Filters> $<TARGET_FILE_DIR:Filters>
	COMMAND_EXPAND_LISTS
	)
endif()

catch_discover_tests(Filters)

add_dependencies(Filters
	ngprotoshaders
	nghalshaders
	)
