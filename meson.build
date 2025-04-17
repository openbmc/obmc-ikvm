project(
    'obmc-ikvm',
    'cpp',
    version: '1.0',
    license: 'GPLv2',
    default_options: ['cpp_std=c++23', 'werror=true'],
    meson_version: '>=1.1.1',
)

usb_vhub_path = get_option('usb-vhub-path')
if usb_vhub_path == 'ast2700_vhuba0'
    add_project_arguments('-DAST2700_VHUBA0', language: 'cpp')
endif

install_data(
    'create_usbhid.sh',
    install_mode: 'rwxr-xr-x',
    install_dir: get_option('bindir'),
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
        dependency('boost'),
    ],
    install: true,
)

fs = import('fs')
fs.copyfile(
    'obmc-ikvm.service',
    install: true,
    install_dir: dependency('systemd').get_variable('systemdsystemunitdir'),
)
