file(GLOB lsdn_SRC "*.c")
file(GLOB lsdn_PUBLIC "include/*.h")
file(GLOB lsdn_PRIVATE "private/*.h")

include_directories(${MNL_INCLUDE_DIRS} ${KERNEL_HEADERS} ${UTHASH_INCLUDE_DIR})

add_library(lsdn
   ${lsdn_SRC}
   ${lsdn_PUBLIC}
   ${lsdn_PRIVATE}
)
target_link_libraries(
	lsdn
	${MNL_LIBRARIES} pthread
)
set_target_properties(lsdn PROPERTIES PUBLIC_HEADER
	"include/errors.h;include/lsdn.h;include/nettypes.h;include/util.h;")

install(
	TARGETS lsdn
	LIBRARY DESTINATION lib
	PUBLIC_HEADER DESTINATION include/lsdn
)

set_source_files_properties(tags PROPERTIES GENERATED true)
add_custom_target(tags
    COMMAND ctags ${lsdn_SRC} ${lsdn_PUBLIC} ${lsdn_PRIVATE}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
