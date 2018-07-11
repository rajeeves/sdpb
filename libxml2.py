#! /usr/bin/env python
# encoding: utf-8

def configure(conf):
    conf.load('compiler_cxx cxx14')
    
    # Find Libxml2
    if conf.options.libxml2_dir:
        if not conf.options.libxml2_incdir:
            conf.options.libxml2_incdir=conf.options.libxml2_dir + "/include"
        if not conf.options.libxml2_libdir:
            conf.options.libxml2_libdir=conf.options.libxml2_dir + "/lib"

    if conf.options.libxml2_incdir:
        libxml2_incdir=conf.options.libxml2_incdir.split()
    else:
        libxml2_incdir=[]
    if conf.options.libxml2_libdir:
        libxml2_libdir=conf.options.libxml2_libdir.split()
    else:
        libxml2_libdir=[]

    if conf.options.libxml2_libs:
        libxml2_libs=conf.options.libxml2_libs.split()
    else:
        libxml2_libs=['xml2']

    if not conf.check_cxx(msg="Checking for libxml2",
                          header_name='libxml/parser.h',
                          includes=libxml2_incdir,
                          uselib_store='libxml2',
                          libpath=libxml2_libdir,
                          rpath=libxml2_libdir,
                          lib=libxml2_libs,
                          mandatory=False) and not conf.check_cfg(path='pkg-config', args='--libs --cflags',
                                                                  package='libxml-2.0', uselib_store='libxml2',
                                                                  mandatory=False):
        conf.fatal("Could not find libxml2")


def options(opt):
    libxml2=opt.add_option_group('Libxml2 Options')
    libxml2.add_option('--libxml2-dir',
                   help='Base directory where libxml2 is installed')
    libxml2.add_option('--libxml2-incdir',
                   help='Directory where libxml2 include files are installed')
    libxml2.add_option('--libxml2-libdir',
                   help='Directory where libxml2 library files are installed')
    libxml2.add_option('--libxml2-libs',
                   help='Names of the libxml2 libraries without prefix or suffix\n'
                   '(e.g. "xml2")')
