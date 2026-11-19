cmake_minimum_required(VERSION 3.25)
project(GeoBRMod VERSION 1.10.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Geode 5.9 requires Clang 19. NDK r28 supplies it, but that NDK's libc++ lacks
# atomic_ref and no longer exposes every trait transitively. Force a narrow
# compatibility header into C++ targets until the upstream dependencies no
# longer rely on those library details.
# NDK r28 libc++ shim: Android only (clang-cl would read the path as a source file).
if(ANDROID)
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/compat/NdkStdCompat.hpp>"
    )
endif()
add_library(${PROJECT_NAME} SHARED
    src/services/AccessService.cpp
    src/ui/AdminSafetyPopup.cpp
    src/ui/ProfilePreviewPopup.cpp
    src/ui/ExtrasPopup.cpp
    src/main.cpp
    src/ui/ProgressionPopup.cpp
    src/ui/SeasonBuilderPopup.cpp
    src/ui/GeoMapPopup.cpp
    src/ui/DirectMessagesPopup.cpp
    src/config/GeoBRConfig.cpp
    src/models/NewsItem.cpp
    src/models/CommunityMessage.cpp
    src/models/MainLevel.cpp
    src/services/ApiClient.cpp
    src/services/CloudService.cpp
    src/services/RankService.cpp
    src/hooks/RankHook.cpp
    src/services/GeoPassService.cpp
    src/services/PresenceService.cpp
    src/hooks/LevelTransferHook.cpp
    src/ui/GeoPassPopup.cpp
    src/ui/GeoBRVisuals.cpp
    src/ui/TierShopPopup.cpp
    src/ui/GeoPlayersPopup.cpp
    src/hooks/ProfileAppearanceHook.cpp
    src/ui/AdminPanel.cpp
    src/ui/AdminForm.cpp
    src/services/PlayerService.cpp
    src/services/NewsService.cpp
    src/services/CommunityService.cpp
    src/services/MainLevelsService.cpp
    src/services/MainLevelLauncher.cpp
    src/services/MainLevelProgress.cpp
    src/ui/UIHelpers.cpp
    src/ui/GeoBRHubLayer.cpp
    src/ui/GeoBRMainLevelsLayer.cpp
    src/ui/NewsTab.cpp
    src/ui/CommunityTab.cpp
    src/hooks/CreatorLayerHook.cpp
    src/hooks/MainLevelProgressHook.cpp
    src/gameplay/GeoBRRobotPack.cpp
    src/utils/TextSanitizer.cpp
    src/arrow/ArrowSkins.cpp
    src/arrow/ArrowPlayer.cpp
    src/arrow/ArrowPortal.cpp
    src/arrow/ArrowLevel.cpp
    src/arrow/ArrowPopup.cpp
    src/arrow/ArrowProfile.cpp
    src/arrow/ArrowGarage.cpp
    src/godeditor/GodEditor.cpp
    src/collab/CollabService.cpp
    src/collab/CollabHooks.cpp
    src/collab/CollabLive.cpp
    src/ui/CollabPopup.cpp
    src/ui/SocialPopups.cpp
    src/gdps/GdpsService.cpp
    src/gdps/GdpsHooks.cpp
    src/ui/GdpsPopups.cpp
    src/ui/GdpsAdminPopup.cpp
    src/ui/GdpsModPopups.cpp
    src/ui/GdpsRatePopup.cpp
    src/gdps/GdpsModHooks.cpp
    src/services/UpdateService.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
if(NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "GEODE_SDK is not set. Point it to the Geode SDK directory.")
endif()
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
setup_geode_mod(${PROJECT_NAME} DONT_INSTALL)
