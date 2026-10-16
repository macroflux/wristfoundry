#pragma once

#include "app_manager.h"

void settings_app_create(wf_app_t *app);
void settings_app_show(wf_app_t *app);
void settings_app_hide(wf_app_t *app);
void settings_app_update(wf_app_t *app, const void *model);
void settings_app_destroy(wf_app_t *app);
