cmake_minimum_required(VERSION 3.27)
project(KatsodiNative VERSION 0.1.0 LANGUAGES CXX)

# Katsodi's own desktop client: C++20, Qt Quick/QML. Nothing here renders
# website pages; data comes from the versioned native API of the server.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# development: API origin from KATSODI_API_ORIGIN (loopback only), default
#              http://127.0.0.1:8787, separate settings and credentials.
# production:  the fixed origin below; nothing at runtime can change it.
set(KATSODI_CHANNEL "development" CACHE STRING "development or production")
set_property(CACHE KATSODI_CHANNEL PROPERTY STRINGS development production)
set(KATSODI_PRODUCTION_ORIGIN "https://katsodi.watch")

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 Sql Multimedia Test)

# Voice: the official LiveKit C++ SDK, prebuilt (tools/install-livekit.py).
# Without it the client still builds and shows voice as unavailable.
find_package(LiveKit 1.13 CONFIG QUIET PATHS "$ENV{KATSODI_LIVEKIT_DIR}" NO_DEFAULT_PATH)
# The SDK ships a Release DLL only and passes std::string across its API, so
# the client links against the release C runtime (Debug is refused).
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
if(LiveKit_FOUND AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(FATAL_ERROR "With LiveKit use RelWithDebInfo or Release (the SDK uses the release CRT).")
endif()
message(STATUS "LiveKit voice: ${LiveKit_FOUND}")
qt_standard_project_setup(REQUIRES 6.11)

execute_process(
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE KATSODI_REVISION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
if(NOT KATSODI_REVISION)
  set(KATSODI_REVISION "unknown")
endif()
configure_file(src/app/BuildConfig.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/BuildConfig.h @ONLY)

# First-party warnings only; third-party targets keep their own flags.
function(katsodi_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

# --- Core: domain, infrastructure and presentation models (no QML) -------
qt_add_library(katsodi_core STATIC
  src/domain/Text.h src/domain/Text.cpp
  src/domain/Records.h src/domain/Records.cpp
  src/infrastructure/api/ApiClient.h src/infrastructure/api/ApiClient.cpp
  src/infrastructure/credentials/CredentialStore.h src/infrastructure/credentials/CredentialStore.cpp
  src/application/Session.h src/application/Session.cpp
  src/presentation/Preferences.h src/presentation/Preferences.cpp
  src/presentation/ThemeController.h src/presentation/ThemeController.cpp
  src/presentation/Strings.h src/presentation/Strings.cpp
  src/presentation/FandomModel.h src/presentation/FandomModel.cpp
  src/presentation/RoomListModel.h src/presentation/RoomListModel.cpp
  src/presentation/ChatModel.h src/presentation/ChatModel.cpp
  src/presentation/RoomController.h src/presentation/RoomController.cpp
  src/presentation/VoiceController.h src/presentation/VoiceController.cpp
  src/presentation/SearchModel.h src/presentation/SearchModel.cpp
  src/presentation/PlayerController.h src/presentation/PlayerController.cpp
  src/infrastructure/storage/LocalLibrary.h src/infrastructure/storage/LocalLibrary.cpp
  src/media/VoiceEngine.h
  src/media/ScreenEngine.h
  src/presentation/ScreenController.h src/presentation/ScreenController.cpp
)
if(LiveKit_FOUND)
  target_sources(katsodi_core PRIVATE
    src/media/livekit/LiveKitSdk.h src/media/livekit/LiveKitSdk.cpp
    src/media/livekit/LiveKitVoiceEngine.h src/media/livekit/LiveKitVoiceEngine.cpp
    src/media/livekit/LiveKitScreenEngine.h src/media/livekit/LiveKitScreenEngine.cpp)
  target_link_libraries(katsodi_core PUBLIC LiveKit::livekit)
  target_compile_definitions(katsodi_core PUBLIC KATSODI_WITH_LIVEKIT=1)
endif()
target_include_directories(katsodi_core PUBLIC src ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(katsodi_core PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Sql Qt6::Multimedia)
if(WIN32)
  target_link_libraries(katsodi_core PUBLIC advapi32)
endif()
qt_add_resources(katsodi_core "katsodi_data"
  PREFIX "/katsodi"
  BASE resources
  FILES
    resources/generated/strings.json
    resources/generated/palettes.json
    resources/generated/fandoms.json
    resources/fonts/manrope-latin.ttf
    resources/fonts/manrope-cyrillic.ttf
    resources/fonts/manrope-latin-ext.ttf
    resources/fonts/manrope-cyrillic-ext.ttf
    resources/fonts/Oswald.ttf
    resources/fonts/Caveat.ttf
    resources/icons/logo.svg
)
katsodi_warnings(katsodi_core)

# --- Tests ----------------------------------------------------------------
enable_testing()
qt_add_executable(tst_core tests/unit/tst_core.cpp src/app/PosterImageProvider.cpp)
target_link_libraries(tst_core PRIVATE katsodi_core Qt6::Quick Qt6::Test)
katsodi_warnings(tst_core)
set_target_properties(tst_core PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME core COMMAND tst_core)
if(LiveKit_FOUND)
  # Live checks against a real LiveKit server: tools/voice-live-check.mjs and
  # tools/screen-live-check.mjs (LiveKit DLLs are copied next to tst_core).
  qt_add_executable(tst_voice_live tests/live/tst_voice_live.cpp)
  target_link_libraries(tst_voice_live PRIVATE katsodi_core Qt6::Test)
  set_target_properties(tst_voice_live PROPERTIES WIN32_EXECUTABLE FALSE)
  katsodi_warnings(tst_voice_live)
  qt_add_executable(tst_screen_live tests/live/tst_screen_live.cpp)
  target_link_libraries(tst_screen_live PRIVATE katsodi_core Qt6::Test)
  set_target_properties(tst_screen_live PROPERTIES WIN32_EXECUTABLE FALSE)
  katsodi_warnings(tst_screen_live)
endif()
set_tests_properties(core PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# --- Application ----------------------------------------------------------
qt_add_executable(katsodi WIN32 src/app/main.cpp src/app/katsodi.rc
  src/app/PosterImageProvider.h src/app/PosterImageProvider.cpp)
set_target_properties(katsodi PROPERTIES OUTPUT_NAME "Katsodi")
set_source_files_properties(qml/theme/K.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(katsodi
  URI Katsodi.Ui
  VERSION 1.0
  QML_FILES
    qml/Main.qml
    qml/theme/K.qml
    qml/controls/Glass.qml
    qml/controls/Icon.qml
    qml/controls/IconButton.qml
    qml/controls/KButton.qml
    qml/controls/KTextField.qml
    qml/controls/Segmented.qml
    qml/controls/PageHero.qml
    qml/controls/SectionTitle.qml
    qml/controls/Poster.qml
    qml/controls/Logo.qml
    qml/controls/Scroller.qml
    qml/controls/HRow.qml
    qml/shell/Backdrop.qml
    qml/shell/Rail.qml
    qml/shell/NowBar.qml
    qml/shell/SearchOverlay.qml
    qml/screens/HomeScreen.qml
    qml/screens/RoomsScreen.qml
    qml/screens/RoomCard.qml
    qml/screens/FandomCard.qml
    qml/screens/RoomScreen.qml
    qml/screens/PlayerStage.qml
    qml/screens/DemoPanel.qml
    qml/screens/VoicePanel.qml
    qml/screens/FandomsScreen.qml
    qml/screens/LibraryScreen.qml
    qml/screens/CreateRoomDialog.qml
    qml/screens/SettingsScreen.qml
    qml/screens/SignInScreen.qml
)
target_link_libraries(katsodi PRIVATE katsodi_core Qt6::Quick Qt6::QuickControls2)
katsodi_warnings(katsodi)

# Fandom posters listed in the exported catalogue, copied next to the
# executable (not embedded, so they are decoded only when shown).
file(READ resources/generated/fandoms.json KATSODI_FANDOMS)
string(JSON KATSODI_FANDOM_COUNT LENGTH "${KATSODI_FANDOMS}")
math(EXPR KATSODI_FANDOM_LAST "${KATSODI_FANDOM_COUNT} - 1")
set(KATSODI_ARTWORK "")
foreach(index RANGE ${KATSODI_FANDOM_LAST})
  string(JSON art_type ERROR_VARIABLE art_error TYPE "${KATSODI_FANDOMS}" ${index} art)
  if(art_type STREQUAL "OBJECT")
    string(JSON art_file GET "${KATSODI_FANDOMS}" ${index} art file)
    list(APPEND KATSODI_ARTWORK "${CMAKE_CURRENT_SOURCE_DIR}/../public/images/fandoms/${art_file}")
  endif()
endforeach()
if(LiveKit_FOUND)
  # livekit.dll and livekit_ffi.dll next to the executable, plus the licence.
  get_target_property(KATSODI_LIVEKIT_DLL LiveKit::livekit IMPORTED_LOCATION_RELEASE)
  get_filename_component(KATSODI_LIVEKIT_BIN "${KATSODI_LIVEKIT_DLL}" DIRECTORY)
  get_filename_component(KATSODI_LIVEKIT_ROOT "${KATSODI_LIVEKIT_BIN}" DIRECTORY)
  set(KATSODI_LIVEKIT_DLLS "${KATSODI_LIVEKIT_BIN}/livekit.dll" "${KATSODI_LIVEKIT_BIN}/livekit_ffi.dll")
  add_custom_command(TARGET katsodi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${KATSODI_LIVEKIT_DLLS} $<TARGET_FILE_DIR:katsodi>
    VERBATIM)
  add_custom_command(TARGET tst_core POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${KATSODI_LIVEKIT_DLLS} $<TARGET_FILE_DIR:tst_core>
    VERBATIM)
  install(FILES ${KATSODI_LIVEKIT_DLLS} DESTINATION bin)
  install(FILES "${KATSODI_LIVEKIT_ROOT}/LICENSE" DESTINATION licenses RENAME LiveKit-SDK-LICENSE.txt)
endif()

set(KATSODI_EDITORIAL "${CMAKE_CURRENT_SOURCE_DIR}/resources/artwork/space-1600.webp" "${CMAKE_CURRENT_SOURCE_DIR}/resources/artwork/forest-1600.webp")
add_custom_command(TARGET katsodi POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:katsodi>/artwork/editorial
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${KATSODI_EDITORIAL} $<TARGET_FILE_DIR:katsodi>/artwork/editorial
  VERBATIM)
install(FILES ${KATSODI_EDITORIAL} DESTINATION bin/artwork/editorial)
add_custom_command(TARGET katsodi POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:katsodi>/artwork/fandoms
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${KATSODI_ARTWORK} $<TARGET_FILE_DIR:katsodi>/artwork/fandoms
  COMMAND_EXPAND_LISTS
  VERBATIM)

# The executable sits with the Qt libraries in bin/ (the deploy script puts
# them there and writes bin/qt.conf); artwork lives next to the executable.
install(TARGETS katsodi RUNTIME DESTINATION bin)
install(FILES ${KATSODI_ARTWORK} DESTINATION bin/artwork/fandoms)
install(FILES resources/fonts/OFL.txt DESTINATION licenses RENAME Manrope-OFL.txt)
install(FILES resources/fonts/Oswald-OFL.txt resources/fonts/Caveat-OFL.txt resources/artwork/CREDITS.txt DESTINATION licenses)
qt_generate_deploy_qml_app_script(
  TARGET katsodi
  OUTPUT_SCRIPT KATSODI_DEPLOY_SCRIPT
  NO_UNSUPPORTED_PLATFORM_ERROR
  # No QML debugger/profiler plugins in a release.
  DEPLOY_TOOL_OPTIONS --no-translations --no-system-d3d-compiler --no-opengl-sw --skip-plugin-types qmltooling
)
install(SCRIPT ${KATSODI_DEPLOY_SCRIPT})

# --- Provider-player helper (optional) ------------------------------------
# A separate process hosting official provider embeds in Microsoft Edge
# WebView2 (tools/install-webview2.py); the client starts it on demand
# (src/presentation/PlayerController). Without the SDK the client builds
# and offers "open in browser" only.
file(TO_CMAKE_PATH "$ENV{KATSODI_WEBVIEW2_DIR}" KATSODI_WEBVIEW2_DIR)
if(WIN32 AND EXISTS "${KATSODI_WEBVIEW2_DIR}/build/native/include/WebView2.h")
  set(KATSODI_PLAYER TRUE)
else()
  set(KATSODI_PLAYER FALSE)
endif()
message(STATUS "Provider player helper (WebView2): ${KATSODI_PLAYER}")
if(KATSODI_PLAYER)
  qt_add_executable(katsodi_player WIN32 src/provider-helper/main.cpp)
  set_target_properties(katsodi_player PROPERTIES OUTPUT_NAME "katsodi-player")
  target_include_directories(katsodi_player PRIVATE "${KATSODI_WEBVIEW2_DIR}/build/native/include")
  # The static loader: no WebView2Loader.dll to ship.
  target_link_libraries(katsodi_player PRIVATE Qt6::Core
    "${KATSODI_WEBVIEW2_DIR}/build/native/x64/WebView2LoaderStatic.lib"
    ole32 oleaut32 user32 gdi32 advapi32 shell32 version)
  katsodi_warnings(katsodi_player)
  add_dependencies(katsodi katsodi_player)
  target_compile_definitions(katsodi PRIVATE KATSODI_WITH_PLAYER=1)
  install(TARGETS katsodi_player RUNTIME DESTINATION bin)
  install(FILES "${KATSODI_WEBVIEW2_DIR}/LICENSE.txt" DESTINATION licenses RENAME WebView2-SDK-LICENSE.txt)
  # Live check against the real providers (needs npm run native:api).
  qt_add_executable(tst_player_live tests/live/tst_player_live.cpp)
  target_link_libraries(tst_player_live PRIVATE Qt6::Quick Qt6::Test user32 gdi32)
  set_target_properties(tst_player_live PROPERTIES WIN32_EXECUTABLE FALSE)
  add_dependencies(tst_player_live katsodi_player)
endif()
