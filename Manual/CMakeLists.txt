
pod2man(dealer.pod dealer 6 ${PROJECT_DESCRIPTION})
pod2man(dealer.dpp.pod dealer.dpp 6 ${PROJECT_DESCRIPTION})

install(FILES 
    authors.html
    bugs.html
    command.html
    disclaimers.html
    download.html
    index.gif
    index.html
    input.html
    manual.html
    next.gif
    output.html
    perf.html
    preproc.html
    prev.gif
    running.html
    updates.html
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/Manual)
