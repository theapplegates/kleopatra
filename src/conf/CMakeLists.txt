include_directories(${kleopatra_SOURCE_DIR}/src)

set(kcm_kleopatra_PART_SRCS
  dirservconfigpage.cpp
  appearanceconfigpage.cpp
  appearanceconfigwidget.cpp
  gnupgsystemconfigurationpage.cpp
  dirservconfigpage.h
  appearanceconfigpage.h
  appearanceconfigwidget.h
  gnupgsystemconfigurationpage.h
  labelledwidget.cpp labelledwidget.h
  labelledwidget.cpp labelledwidget.h
  kleoconfigmodule.cpp
  kleoconfigmodule.h
  ${kleopatra_BINARY_DIR}/src/kleopatra_debug.cpp
  smimevalidationconfigurationwidget.cpp
  smimevalidationconfigurationpage.cpp
  cryptooperationsconfigwidget.cpp
  cryptooperationsconfigpage.cpp
  smimevalidationconfigurationwidget.h
  smimevalidationconfigurationpage.h
  cryptooperationsconfigwidget.h
  cryptooperationsconfigpage.h
  smartcardconfigpage.cpp
  smartcardconfigpage.h
)

ki18n_wrap_ui(kcm_kleopatra_PART_SRCS
  smimevalidationconfigurationwidget.ui
)

kconfig_add_kcfg_files(kcm_kleopatra_PART_SRCS
  ${kleopatra_SOURCE_DIR}/src/kcfg/settings.kcfgc
)

add_library(kcm_kleopatra_static STATIC ${kcm_kleopatra_PART_SRCS})
set_property(TARGET kcm_kleopatra_static PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(kcm_kleopatra_static
  KPim6::Libkleo
  KF6::ConfigGui
  KF6::I18n
  KF6::IconThemes
  KF6::IconWidgets
  KF6::WidgetsAddons
  ${_kleopatra_dbusaddons_libs}
)
