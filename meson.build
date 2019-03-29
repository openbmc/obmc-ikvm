project(
    'obmc-ikvm',
    'cpp',
    version: '1.0',
    license: 'Apache-2.0',
    default_options: [
        'cpp_std=c++17',
        'werror=true',
    ],
)

executable(
    'obmc-ikvm',
    [
        'ikvm_args.cpp',
        'ikvm_input.cpp',
        'ikvm_manager.cpp',
        'ikvm_server.cpp',
        'ikvm_video.cpp',
        'obmc-ikvm.cpp',
    ],
    dependencies: [
        dependency('libvncserver'),
        dependency('phosphor-logging'),
        dependency('phosphor-dbus-interfaces'),
        dependency('sdbusplus'),
        dependency('threads'),
    ],
    install: true
)
