version_xml = configure_file(input: 'version.xml.in',
    output: 'version.xml',
    configuration: conf)

gnome.gtkdoc('libmediaart',
    src_dir: 'libmediaart',
    main_sgml: 'libmediaart.sgml',
    install: true)
