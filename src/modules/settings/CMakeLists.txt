set(extra_deps
	email-engine
	evolution-calendar
	evolution-mail
	evolution-mail-formatter
	evolution-shell
)
set(sources
	evolution-module-settings.c
	e-settings-cal-model.c
	e-settings-cal-model.h
	e-settings-calendar-item.c
	e-settings-calendar-item.h
	e-settings-calendar-view.c
	e-settings-calendar-view.h
	e-settings-client-cache.c
	e-settings-client-cache.h
	e-settings-date-edit.c
	e-settings-date-edit.h
	e-settings-deprecated.c
	e-settings-deprecated.h
	e-settings-content-editor.c
	e-settings-content-editor.h
	e-settings-mail-browser.c
	e-settings-mail-browser.h
	e-settings-mail-formatter.c
	e-settings-mail-formatter.h
	e-settings-mail-part-headers.c
	e-settings-mail-part-headers.h
	e-settings-mail-reader.c
	e-settings-mail-reader.h
	e-settings-mail-session.c
	e-settings-mail-session.h
	e-settings-meeting-store.c
	e-settings-meeting-store.h
	e-settings-meeting-time-selector.c
	e-settings-meeting-time-selector.h
	e-settings-message-list.c
	e-settings-message-list.h
	e-settings-name-selector-entry.c
	e-settings-name-selector-entry.h
	e-settings-spell-checker.c
	e-settings-spell-checker.h
	e-settings-spell-entry.c
	e-settings-spell-entry.h
	e-settings-weekday-chooser.c
	e-settings-weekday-chooser.h
)
set(extra_defines)
set(extra_cflags)
set(extra_incdirs)
set(extra_ldflags)

add_evolution_module(module-settings
	sources
	extra_deps
	extra_defines
	extra_cflags
	extra_incdirs
	extra_ldflags
)
