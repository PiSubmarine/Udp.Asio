include("${PISUBMARINE_BUILD_SYSTEM}")

message("Adding library: ${PISUBMARINE_MODULE_NAME}")
add_library(${PISUBMARINE_MODULE_NAME}
        PiSubmarine/Udp/Asio/Socket.cpp)
target_include_directories(${PISUBMARINE_MODULE_NAME} PUBLIC "../public")
PiSubmarineInitTarget(${PISUBMARINE_MODULE_NAME})

find_package(Boost REQUIRED COMPONENTS system)

PiSubmarineAddDependency("https://github.com/PiSubmarine/Udp.Api.git" "")
target_link_libraries(${PISUBMARINE_MODULE_NAME} PUBLIC PiSubmarine.Udp.Api)
target_link_libraries(${PISUBMARINE_MODULE_NAME} PUBLIC Boost::system)
