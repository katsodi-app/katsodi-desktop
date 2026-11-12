cmake_minimum_required(VERSION 3.27)
project(KatsodiNative VERSION 0.1.0 LANGUAGES CXX)

# Katsodi's own desktop client: C++20, Qt Quick/QML. Nothing here renders
# website pages; data comes from the versioned native API of the server.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# development: API origin from KATSODI_API_ORIGIN (loopback only), default
#              http://127.0.0.1:3000, separate settings and credentials.
# production:  the fixed origin below; nothing at runtime can change it.
set(KATSODI_CHANNEL "development" CACHE STRING "development or production")
set_property(CACHE KATSODI_CHANNEL PROPERTY STRINGS development production)
set(KATSODI_PRODUCTION_ORIGIN "https://katsodi.watch")

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 Test)
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
)
target_include_directories(katsodi_core PUBLIC src ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(katsodi_core PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml)
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
    resources/icons/logo.svg
)
katsodi_warnings(katsodi_core)

# --- Application ----------------------------------------------------------
qt_add_executable(katsodi WIN32 src/app/main.cpp src/app/katsodi.rc)
set_target_properties(katsodi PROPERTIES OUTPUT_NAME "Katsodi")
qt_add_qml_module(katsodi
  URI Katsodi.Ui
  VERSION 1.0
  QML_FILES
    qml/Main.qml
    qml/controls/KButton.qml
    qml/controls/KChip.qml
    qml/controls/KTextField.qml
    qml/controls/PageIntro.qml
    qml/controls/Poster.qml
    qml/shell/Sidebar.qml
    qml/shell/NavItem.qml
    qml/shell/TopBar.qml
    qml/screens/HomeScreen.qml
    qml/screens/RoomsScreen.qml
    qml/screens/RoomCard.qml
    qml/screens/RoomScreen.qml
    qml/screens/FandomsScreen.qml
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
qt_generate_deploy_qml_app_script(
  TARGET katsodi
  OUTPUT_SCRIPT KATSODI_DEPLOY_SCRIPT
  NO_UNSUPPORTED_PLATFORM_ERROR
  # No QML debugger/profiler plugins in a release.
  DEPLOY_TOOL_OPTIONS --no-translations --no-system-d3d-compiler --no-opengl-sw --skip-plugin-types qmltooling
)
install(SCRIPT ${KATSODI_DEPLOY_SCRIPT})

# --- Tests ----------------------------------------------------------------
enable_testing()
qt_add_executable(tst_core tests/unit/tst_core.cpp)
target_link_libraries(tst_core PRIVATE katsodi_core Qt6::Test)
katsodi_warnings(tst_core)
add_test(NAME core COMMAND tst_core)
set_tests_properties(core PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
