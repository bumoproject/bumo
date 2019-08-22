#bumo main module CmakeLists.txt -- bumo

set(APP_BUMO notary)

set(APP_BUMO_SRC
    main.cpp
    configure.cpp
	chain_rpc.cpp
	http_client.cpp
	notary_mgr.cpp
)


set(INNER_LIBS bumo_common bumo_utils bumo_proto)

#Generate executable files
add_executable(${APP_BUMO} ${APP_BUMO_SRC})

#Specify dependent libraries for target objects
	

MESSAGE(STATUS "current platform: Linux ")  
target_link_libraries(${APP_BUMO}
-Wl,-dn ${INNER_LIBS} -Wl,--start-group -Wl,--end-group ${BUMO_DEPENDS_LIBS} ${BUMO_LINKER_FLAGS})



#Specify compiling options for target objets
target_compile_options(${APP_BUMO}
    PUBLIC -std=c++11 
    PUBLIC -DASIO_STANDALONE
    PUBLIC -D_WEBSOCKETPP_CPP11_STL_
    PUBLIC -D${OS_NAME}
)

install(TARGETS ${APP_BUMO}
    RUNTIME DESTINATION ${BUMOCHAIN}/bin
)
