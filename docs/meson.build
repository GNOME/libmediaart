if get_option('gtk_doc')
    version_xml = configure_file(input: 'version.xml.in',
        output: 'version.xml',
        configuration: conf)

    ignored_headers = [
        'marshal.h',
    ]

    ignored_decorators = [
        '_LIBMEDIAART_EXTERN',
    ]

    gnome.gtkdoc('libmediaart',
        namespace: 'media_art',
        src_dir: include_directories('../libmediaart'),
        main_xml: 'libmediaart-docs.xml',
        ignore_headers: ignored_headers,
        scan_args: [
            '--rebuild-types',
            '--ignore-decorators=' + '|'.join(ignored_decorators),
        ],
        mkdb_args: [
            '--default-include=libmediaart/mediaart.h',
        ],
        content_files: ['overview.xml'],
        dependencies: libmediaart_dep,
        install: true,
        check: true)
endif
