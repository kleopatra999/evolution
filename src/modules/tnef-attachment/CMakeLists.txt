set(extra_deps
	evolution-mail
	evolution-mail-formatter
)
set(sources
	e-mail-parser-tnef-attachment.c
	e-mail-parser-tnef-attachment.h
	evolution-module-tnef-attachment.c
)
set(extra_defines)
set(extra_cflags)
set(extra_incdirs)
set(extra_ldflags
	${TNEF_LDFLAGS}
)

add_evolution_module(module-tnef-attachment
	sources
	extra_deps
	extra_defines
	extra_cflags
	extra_incdirs
	extra_ldflags
)
