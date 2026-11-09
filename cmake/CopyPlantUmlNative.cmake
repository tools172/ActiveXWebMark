# Copies the fetched PlantUML headless native files next to the application.
#
# The application builds and runs without them (PlantUML falls back to the
# remote server or source text), so a missing staging directory is not an
# error here. Packaging has its own hard requirement.

if(EXISTS "${SRC}/plantuml-headless.exe")
    file(REMOVE_RECURSE "${DST}")
    file(MAKE_DIRECTORY "${DST}")
    file(COPY "${SRC}/" DESTINATION "${DST}")
endif()
