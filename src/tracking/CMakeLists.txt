add_library(macroman_tracking STATIC
    DataAssociation.cpp
    KalmanPredictor.cpp
    TargetSelector.cpp
    TargetTracker.cpp
)

target_include_directories(macroman_tracking PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(macroman_tracking PUBLIC
    macroman_core
    Eigen3::Eigen
)
