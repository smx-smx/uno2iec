find_program(ACME_COMMAND acme REQUIRED)

function(acme)
	cmake_parse_arguments(arg "" "FORMAT;INPUT;OUTPUT;TARGET" "" ${ARGN})
	add_custom_command(
		OUTPUT ${arg_OUTPUT}
		COMMAND ${ACME_COMMAND} -f ${arg_FORMAT} -o ${arg_OUTPUT} ${arg_INPUT}
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	)
	add_custom_target(${arg_TARGET} DEPENDS ${arg_OUTPUT})
endfunction()

function(bin_to_array)
	cmake_parse_arguments(arg "" "NAME;INPUT;OUTPUT;TARGET" "" ${ARGN})
	if(NOT TARGET bin_to_array)
		add_executable(bin_to_array bin_to_array.cc)
	endif()

	add_custom_command(
		OUTPUT ${arg_OUTPUT}
		COMMAND $<TARGET_FILE:bin_to_array> ${arg_NAME}_bin ${arg_INPUT} ${arg_OUTPUT}
	)
	add_custom_target(${arg_TARGET} DEPENDS ${arg_OUTPUT})
endfunction()

set(SRCDIR ${CMAKE_CURRENT_LIST_DIR})
set(BINDIR ${CMAKE_CURRENT_BINARY_DIR})

acme(
	FORMAT plain
	INPUT ${SRCDIR}/format.asm
	OUTPUT ${BINDIR}/format.bin
	TARGET format_bin
)
bin_to_array(
	NAME format
	INPUT ${BINDIR}/format.bin
	OUTPUT ${BINDIR}/format_h.h
	TARGET format_h
)
add_dependencies(format_h format_bin)

acme(
	FORMAT plain
	INPUT ${SRCDIR}/rw_block.asm
	OUTPUT ${BINDIR}/rw_block.bin
	TARGET rw_block_bin
)
bin_to_array(
	NAME rw_block
	INPUT ${BINDIR}/rw_block.bin
	OUTPUT 	${BINDIR}/rw_block_h.h
	TARGET rw_block_h
)
add_dependencies(rw_block_h rw_block_bin)