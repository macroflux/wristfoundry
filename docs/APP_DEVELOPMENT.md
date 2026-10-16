# App development

An app is a static `wf_app_t` descriptor registered during UI startup.

```c
static wf_app_t example = {
    .id = "EXAMPLE",
    .name = "EXAMPLE",
    .icon = "EX",
    .description = "A small demonstration app.",
    .create = example_create,
    .show = example_show,
    .hide = example_hide,
    .update = example_update,
    .destroy = example_destroy,
};

wf_app_manager_register(&example);
```

Lifecycle contract:

- `create` is required and called lazily once before first display. It must set
  `app->screen`.
- `show` runs after the screen becomes active. Acquire sensors or bounded service
  leases here.
- `hide` runs before leaving the app. Stop timers, release radios and sensors,
  and make the app sleep-safe.
- `update` receives the shared read-only watch model while the screen exists.
- `destroy` releases the screen and app-owned context when explicitly invoked.

App IDs must be unique. Apps must use the shared header/back behavior and report
user interaction so inactivity timers remain correct. They must not persist
secrets to SD, hold radios indefinitely, or prevent sleep after `hide`.

The next API revision will add typed capabilities, app-specific model channels,
and compile-time registration helpers. Those are proposed, not implemented.
