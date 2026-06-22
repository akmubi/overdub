#ifndef TOOL_UOBJECT_SEARCH_H
#define TOOL_UOBJECT_SEARCH_H

#include "mod_manager.h"

void
register_builtin_uobject_search(mod_manager_t *manager);
void
register_builtin_ufunction_tracer(mod_manager_t *manager);
void
register_builtin_tweaks(mod_manager_t *manager);

#endif /* TOOL_UOBJECT_SEARCH_H */
