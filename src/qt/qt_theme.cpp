/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Global Qt theme handling.
 *
 * Authors: OpenAI Codex
 *
 *          Copyright 2026 OpenAI
 */

#include "qt_theme.hpp"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QPalette>
#include <QStyle>
#include <QTextStream>
#include <QWidget>

#include <cstdio>

#ifdef Q_OS_WINDOWS
#    include <windows.h>
#    include <dwmapi.h>
#    ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#        define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#    endif
#endif

extern "C" {
#include <86box/86box.h>
}

namespace {

constexpr auto kBlackModernQssPath = ":/themes/black_modern.qss";

QPalette
blackModernPalette()
{
    QPalette palette = qApp->style() ? qApp->style()->standardPalette() : QPalette();

    const QColor window("#0E0E11");
    const QColor surface("#17171C");
    const QColor elevated("#1E1E26");
    const QColor border("#2A2A33");
    const QColor text("#F2F2F5");
    const QColor secondary("#A8A8B3");
    const QColor accent("#4F8CFF");
    const QColor accentVisited("#B57BFF");
    const QColor disabled("#6C6C78");

    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, surface);
    palette.setColor(QPalette::AlternateBase, elevated);
    palette.setColor(QPalette::ToolTipBase, elevated);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, elevated);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, accent);
    palette.setColor(QPalette::Link, accent);
    palette.setColor(QPalette::LinkVisited, accentVisited);
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, text);
    palette.setColor(QPalette::Light, elevated);
    palette.setColor(QPalette::Midlight, border);
    palette.setColor(QPalette::Mid, border);
    palette.setColor(QPalette::Dark, window);
    palette.setColor(QPalette::Shadow, window);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
    palette.setColor(QPalette::PlaceholderText, secondary);
#endif

    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, border);
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, secondary);

    return palette;
}

QString
loadBlackModernStylesheet()
{
    static bool    initialized = false;
    static QString stylesheet;

    if (!initialized) {
        Q_INIT_RESOURCE(black_modern);
        Q_INIT_RESOURCE(darkstyle);

        QFile file(kBlackModernQssPath);
        if (!file.exists()) {
            printf("Unable to set stylesheet, file not found\n");
        } else if (file.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream stream(&file);
            stylesheet = stream.readAll();
        }

        initialized = true;
    }

    return stylesheet;
}

}

namespace theme {

bool
useSystemTheme()
{
    return color_scheme != 2;
}

bool
isDarkThemeActive()
{
    return !useSystemTheme();
}

void
applyAppTheme()
{
    if (!qApp)
        return;

    if (useSystemTheme()) {
        qApp->setStyleSheet(QString());
        qApp->setPalette(qApp->style() ? qApp->style()->standardPalette() : QPalette());
        return;
    }

    qApp->setPalette(blackModernPalette());
    qApp->setStyleSheet(loadBlackModernStylesheet());
}

#ifdef Q_OS_WINDOWS
bool
isSystemThemeChangeMessage(const void *message)
{
    const auto *msg = static_cast<const MSG *>(message);

    return msg
        && (msg->message == WM_SETTINGCHANGE)
        && (msg->lParam != nullptr)
        && (wcscmp(L"ImmersiveColorSet", static_cast<const wchar_t *>(msg->lParam)) == 0);
}

void
syncNativeWindowFrame(QWidget *window)
{
    if (!window)
        return;

    BOOL darkMode = isDarkThemeActive() ? TRUE : FALSE;
    DwmSetWindowAttribute(
        (HWND) window->winId(),
        DWMWA_USE_IMMERSIVE_DARK_MODE,
        (LPCVOID) &darkMode,
        sizeof(darkMode));
}
#endif

}
