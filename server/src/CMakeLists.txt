cmake_minimum_required(VERSION 2.8)

project(assetserver)

include_directories(./)
include_directories(../ethereum)

set(compat_src  compat/glibc_compat.cpp
                compat/glibc_sanity.cpp
                compat/glibcxx_sanity.cpp
                compat/strnlen.cpp)

set(crypto_src  #crypto/aes.cpp
                crypto/chacha20.cpp
                crypto/hmac_sha256.cpp
                crypto/hmac_sha512.cpp
                crypto/poly1305.c
                crypto/ripemd160.cpp
                crypto/sha1.cpp
                crypto/sha256.cpp
                crypto/sha256_sse4.cpp
                crypto/sha512.cpp
                #crypto/ctaes/ctaes.c
                )

set(rpc_src rpc/protocol.cpp
            rpc/server.cpp
			rpc/client.cpp
            rpc/relay.cpp)

set(support_src support/cleanse.cpp
                support/lockedpool.cpp)

set(base_src    
                base58.cpp
                bech32.cpp
                fs.cpp
                hash.cpp
                httprpc.cpp
                httpserver.cpp
                netaddress.cpp
                netbase.cpp           
                random.cpp
                rest.cpp
                sync.cpp
                scheduler.cpp
                threadinterrupt.cpp
                uint256.cpp
                util.cpp
                utilstrencodings.cpp
                utiltime.cpp
                warnings.cpp
                timedata.cpp
				db_mysql.cpp
                ethapi.cpp
                )

find_package(OpenSSL)
set(basic_link_lib curl c rt pthread event event_pthreads gmpxx gmp boost_system boost_chrono boost_program_options python3.6m glog boost_filesystem boost_thread ssl crypto mysqlclient secp256k1 ethereum)

set(asset_src    ${compat_src}
                    ${crypto_src}
                    ${rpc_src}
                    ${support_src}
                    ${base_src}
                    main.cpp)

add_executable(otc-httpserver ${asset_src})

target_link_libraries(otc-httpserver ${basic_link_lib})

