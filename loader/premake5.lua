project 'loader'
    language 'C'
    kind 'ConsoleApp'

    build.compileProperty('x86')
    build.linkingProperty()

    targetname  'loader.bin'
    linkoptions '-T linker.ld -e start'
