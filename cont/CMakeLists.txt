# required for man pages (optional) and base files (mandatory)

### Make base-files (aka sdz's)
add_subdirectory(base)



if (NOT EXISTS ${Spring_BINARY_DIR}/springsettings.cfg)
	if(WIN32)
		set(PLACEHOLDER ";")
	else ()
		set(PLACEHOLDER ":")
	endif ()
	set("SpringDataDirInSource" "${Spring_BINARY_DIR}${PLACEHOLDER}${Spring_SOURCE_DIR}/cont")
	set("FontFile" "${Spring_SOURCE_DIR}/cont/fonts/FreeSansBold.otf")
	set("SmallFontFile" "${Spring_SOURCE_DIR}/cont/fonts/FreeSansBold.otf")
	configure_file(springrc-template-insource.txt ${Spring_BINARY_DIR}/springsettings.cfg)
else ()
	message(STATUS "${Spring_BINARY_DIR}/springsettings.cfg already exists!")
endif ()