if get_option('gtk_doc')
    version_xml = configure_file(input: 'version.xml.in',
        output: 'version.xml',
        configuration: conf)

    ignored_headers = [
        'marshal.h',
    ]

    gnome.gtkdoc('libmediaart',
        namespace: 'media_art',
        src_dir: include_directories('../libmediaart'),
        main_xml: 'libmediaart-docs.xml',
        scan_args: [
            '--ignore-headers=' + ' '.join(ignored_headers),
        ],
        content_files: ['overview.xml'],
        dependencies: libmediaart_dep,
        install: true)
endif
