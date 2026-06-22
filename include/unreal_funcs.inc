#ifndef UE_FUNC_NORMAL
#define UE_FUNC_NORMAL(NAME, RET, ...)
#endif

UE_FUNC_NORMAL(get_pak_signkey_helper, void, void)
UE_FUNC_NORMAL(setup_signed_pak_reader, void *, void *self, void *reader_archive, void *filename);

UE_FUNC_NORMAL(uobject_get_world, uworld_t *, uobject_t *self)
UE_FUNC_NORMAL(static_load_object, uobject_t *, uclass_t *obj_class, uobject_t *outer, const wchar_t *name, const wchar_t *filename, uint32_t load_flags, void *sandbox, bool allow_obj_reconcile, void *instancing_ctx)
UE_FUNC_NORMAL(static_construct_object, uobject_t *, fstatic_construct_obj_params_t *params)
UE_FUNC_NORMAL(create_default_object, uobject_t *, uclass_t *self)
UE_FUNC_NORMAL(static_load_class, uclass_t *, uclass_t *base_cls, uobject_t *outer, const wchar_t *name, const wchar_t *filename, uint32_t load_flags)
UE_FUNC_NORMAL(fname_construct, void, fname_t *self, const wchar_t *name, efind_name_t find_type)
UE_FUNC_NORMAL(add_uobject_delete_listener, void, void *self, fuobject_listener_t *delete_listener)
UE_FUNC_NORMAL(tarray_grow, void *, void *self)
UE_FUNC_NORMAL(tarray_shrink, void *, void *self)
UE_FUNC_NORMAL(get_mcast_sparse_delegate, void *, uobject_t *delegate_owner, fname_t delegate_name)

#undef UE_FUNC_NORMAL

#ifndef UE_FUNC_HOOKED
#define UE_FUNC_HOOKED(NAME, RET, ...)
#endif

UE_FUNC_HOOKED(pak_file_mount, bool, void *self, const wchar_t *pak_filename, int pak_order, const wchar_t *path, bool load_index)
UE_FUNC_HOOKED(io_dispatcher_mount, fio_status_t *, void *self, fio_status_t *status, fio_env_t *env, fguid_t *guid, faes_key_t *key)
UE_FUNC_HOOKED(game_engine_tick, void, void *self, float delta, bool idle_mode);
UE_FUNC_HOOKED(process_event, void, uobject_t *self, ufunc_t *func, void *params)
UE_FUNC_HOOKED(ufunction_invoke, void, ufunc_t *func, uobject_t *obj, fframe_t *stack, void *result);
UE_FUNC_HOOKED(process_mouse_move_event, bool, void *self, fpointer_event_t mouse_event, bool is_synthetic);
UE_FUNC_HOOKED(process_mouse_button_down_event, bool, void *self, void *platform_win, fpointer_event_t mouse_event);
UE_FUNC_HOOKED(process_mouse_button_up_event, bool, void *self, fpointer_event_t mouse_event);
UE_FUNC_HOOKED(process_mouse_button_double_click_event, bool, void *self, void *platform_win, fpointer_event_t mouse_event);
UE_FUNC_HOOKED(process_mouse_wheel_or_gesture_event, bool, void *self, fpointer_event_t wheel_event, fpointer_event_t* gesture_event);
UE_FUNC_HOOKED(process_key_char_event, bool, void *self, fcharacter_event_t character_event);
UE_FUNC_HOOKED(process_key_down_event, bool, void *self, fkey_event_t key_event);
UE_FUNC_HOOKED(process_key_up_event, bool, void *self, fkey_event_t key_event);
UE_FUNC_HOOKED(process_analog_input_event, bool, void *self, fanalog_input_event_t analog_input_event);
UE_FUNC_HOOKED(process_app_activation_event, bool, void *self, bool app_activated);
UE_FUNC_HOOKED(d3d12_viewport_init, void, void *self);

#undef UE_FUNC_HOOKED
