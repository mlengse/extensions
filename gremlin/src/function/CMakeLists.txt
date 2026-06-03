add_library(lbug_gremlin_extension_function
        OBJECT
        gremlin_query.cpp)

set(GREMLIN_EXTENSION_OBJECT_FILES
        ${GREMLIN_EXTENSION_OBJECT_FILES} $<TARGET_OBJECTS:lbug_gremlin_extension_function>
        PARENT_SCOPE)
