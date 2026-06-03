add_library(gremlin_extension_main
        OBJECT
        gremlin_extension.cpp)

set(GREMLIN_EXTENSION_OBJECT_FILES
        ${GREMLIN_EXTENSION_OBJECT_FILES} $<TARGET_OBJECTS:gremlin_extension_main>
        PARENT_SCOPE)
