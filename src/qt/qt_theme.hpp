#ifndef QT_THEME_HPP
#define QT_THEME_HPP

#include <QtGlobal>

class QWidget;

namespace theme {
bool useSystemTheme();
bool isDarkThemeActive();
void applyAppTheme();

#ifdef Q_OS_WINDOWS
bool isSystemThemeChangeMessage(const void *message);
void syncNativeWindowFrame(QWidget *window);
#endif
}

#endif
